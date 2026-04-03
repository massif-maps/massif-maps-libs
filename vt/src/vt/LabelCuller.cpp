#include "LabelCuller.h"

#include <array>
#include <vector>
#include <list>
#include <unordered_map>
#include <memory>

#include <cglib/vec.h>
#include <cglib/mat.h>
#include <cglib/bbox.h>
#include <cglib/frustum3.h>

namespace {
    template <std::size_t N>
    static void gatherPolygonProjectionExtents(const std::array<cglib::vec2<float>, N>& vertList, const cglib::vec2<float>& v, float& outMin, float& outMax) {
        outMin = outMax = cglib::dot_product(v, vertList[0]);
        for (std::size_t i = 1; i < N; ++i) {
            float d = cglib::dot_product(v, vertList[i]);

            if (d < outMin) {
                outMin = d;
            } else if (d > outMax) {
                outMax = d;
            }
        }
    }

    template <std::size_t N>
    static bool testBufferExtent(const std::array<cglib::vec2<float>, N>& vertList, const cglib::vec2<float>& v0, const cglib::vec2<float>& v1, float buffer) {
        if (buffer > 0) {
            cglib::vec2<float> edge = cglib::unit(v1 - v0);
            for (std::size_t i = 0; i < N; ++i) {
                float t = cglib::dot_product(edge, vertList[i] - v0);
                cglib::vec2<float> p = v0 + edge * std::max(0.0f, std::min(1.0f, t));
                float d2 = cglib::norm(vertList[i] - p);
                if (d2 < buffer * buffer) {
                    return false;
                }
            }
        }
        return true;
    }

    template <std::size_t N1, std::size_t N2>
    static bool findSeparatingAxis(const std::array<cglib::vec2<float>, N1>& vertList1, const std::array<cglib::vec2<float>, N2>& vertList2, float buffer) {
        std::size_t i0 = N1 - 1;
        for (std::size_t i1 = 0; i1 < N1; ++i1) {
            cglib::vec2<float> edge = vertList1[i1] - vertList1[i0];
            if (edge == cglib::vec2<float>::zero()) {
                continue;
            }
            cglib::vec2<float> proj(edge(1), -edge(0));

            float min1, max1, min2, max2;
            gatherPolygonProjectionExtents(vertList1, proj, min1, max1);
            gatherPolygonProjectionExtents(vertList2, proj, min2, max2);
            if (max1 < min2 || min1 > max2) {
                if (testBufferExtent(vertList2, vertList1[i0], vertList1[i1], buffer)) {
                    return true;
                }
            }

            i0 = i1;
        }
        return false;
    }

    template <std::size_t N1, std::size_t N2>
    static bool testPolygonOverlap(const std::array<cglib::vec2<float>, N1>& vertList1, const std::array<cglib::vec2<float>, N2>& vertList2, float buffer) {
        return !findSeparatingAxis(vertList1, vertList2, buffer) && !findSeparatingAxis(vertList2, vertList1, buffer);
    }
}

namespace carto::vt {
    LabelCuller::LabelCuller(float scale) :
        _localCameraProjMatrix(cglib::mat4x4<float>::identity()), _scale(scale), _mutex()
    {
    }

    void LabelCuller::setViewState(const ViewState& viewState) {
        std::lock_guard<std::mutex> lock(_mutex);

        cglib::mat4x4<double> localCameraMatrix = viewState.cameraMatrix;
        for (int i = 0; i < 3; i++) {
            localCameraMatrix(i, 3) = 0;
        }
        _localCameraProjMatrix = cglib::mat4x4<float>::convert(viewState.projectionMatrix * localCameraMatrix);
        _viewState = viewState;
        _viewState.zoomScale *= _scale;
    }

    void LabelCuller::reset() {
        std::lock_guard<std::mutex> lock(_mutex);

        clearGrid();
    }

    bool LabelCuller::process(const std::vector<std::shared_ptr<Label>>& labelList, std::mutex& labelMutex) {


        std::lock_guard<std::mutex> lock(_mutex);

        // Start by collecting valid labels and updating label placements
        std::vector<LabelInfo> validLabelList;
        validLabelList.reserve(labelList.size());
        for (const std::shared_ptr<Label>& label : labelList) {
            std::lock_guard<std::mutex> labelLock(labelMutex);

            // Analyze only active and valid labels
            if (!label->isActive()) {
                continue;
            }

            if (label->updatePlacement(_viewState)) {
                label->setOpacity(0);
            }

            if (label->isValid()) {
                CullRecord cullRecord;
                bool valid = calculateScreenEnvelope(label, cullRecord.envelope);
                cullRecord.bounds = cglib::bbox2<float>::make_union(cullRecord.envelope.begin(), cullRecord.envelope.end());
                
                // Calculate screen center position for clustering
                cglib::vec2<float> screenPos = (cullRecord.envelope[0] + cullRecord.envelope[1] + cullRecord.envelope[2] + cullRecord.envelope[3]) * 0.25f;
                
                validLabelList.push_back({ valid, label->getPriority(), label->getLayerIndex(), label->getStyle()->sizeFunc(_viewState), label->getOpacity(), label, cullRecord, screenPos });
            }
        }

        // Perform clustering before standard culling
        performClustering(validLabelList, labelMutex);

        // Sort active labels by priority/size/opacity
        std::stable_sort(validLabelList.begin(), validLabelList.end(), [&](const LabelInfo& labelInfo1, const LabelInfo& labelInfo2) {
            if (labelInfo1.priority != labelInfo2.priority) {
                return labelInfo1.priority > labelInfo2.priority;
            }
            if (labelInfo1.layerIndex != labelInfo2.layerIndex) {
                return labelInfo1.layerIndex < labelInfo2.layerIndex;
            }
            if (labelInfo1.size != labelInfo2.size) {
                return labelInfo1.size > labelInfo2.size;
            }
            if (labelInfo1.opacity != labelInfo2.opacity) {
                return labelInfo1.opacity > labelInfo2.opacity;
            }
            return labelInfo1.label->getGlobalId() > labelInfo2.label->getGlobalId();
        });

        // Update label visibility flag based on overlap analysis
        std::unordered_map<long long, std::vector<const LabelInfo*>> groupMap;
        groupMap.reserve(validLabelList.size());
        bool changed = false;
        for (const LabelInfo& labelInfo : validLabelList) {
            std::lock_guard<std::mutex> labelLock(labelMutex);

            const std::shared_ptr<Label>& label = labelInfo.label;

            long long groupId = label->getGroupId();

            // Label is always visible if its group is set to negative value. Otherwise test visibility against other labels
            bool visible = groupId >= 0 ? labelInfo.valid && testGridOverlap(labelInfo) : labelInfo.valid;

            if (visible && groupId > 0) {
                for (const LabelInfo* otherLabelInfo : groupMap[groupId]) {
                    const std::shared_ptr<Label>& otherLabel = otherLabelInfo->label;
                    float minimumDistance = std::min(label->getMinimumGroupDistance(), otherLabel->getMinimumGroupDistance());
                    if ((!labelInfo.label->allowOverlapSameFeatureId() || !otherLabel->allowOverlapSameFeatureId() ||  labelInfo.label->getLocalId() != otherLabel->getLocalId()) && testPolygonOverlap(labelInfo.cullRecord.envelope, otherLabelInfo->cullRecord.envelope, minimumDistance)) {
                        visible = false;
                        break;
                    }
                }
            }

            if (visible) {
                if (groupId >= 0) {
                    addGridRecord(labelInfo);
                }
                if (groupId > 0) {
                    groupMap[groupId].push_back(&labelInfo);
                }
            }
            if (visible != label->isVisible()) {
                label->setVisible(visible);
                changed = true;
            }
        }
        return changed;
    }

    cglib::vec2<int> LabelCuller::getGridIndex(const cglib::vec2<float>& pos) const {
        int x = std::max(0, std::min(GRID_RESOLUTION_X - 1, static_cast<int>(GRID_RESOLUTION_X * pos(0) / _viewState.resolution / _viewState.aspect)));
        int y = std::max(0, std::min(GRID_RESOLUTION_Y - 1, static_cast<int>(GRID_RESOLUTION_Y * pos(1) / _viewState.resolution)));
        return cglib::vec2<int>(x, y);
    }

    void LabelCuller::clearGrid() {
        for (int y = 0; y < GRID_RESOLUTION_Y; y++) {
            for (int x = 0; x < GRID_RESOLUTION_X; x++) {
                _recordGrid[y][x].clear();
            }
        }
    }

    void LabelCuller::addGridRecord(const LabelInfo& labelInfo) {
        cglib::vec2<int> minPos = getGridIndex(labelInfo.cullRecord.bounds.min);
        cglib::vec2<int> maxPos = getGridIndex(labelInfo.cullRecord.bounds.max);
        for (int y = minPos(1); y <= maxPos(1); y++) {
            for (int x = minPos(0); x <= maxPos(0); x++) {
                _recordGrid[y][x].push_back(labelInfo);
            }
        }
    }

    bool LabelCuller::testGridOverlap(const LabelInfo& labelInfo) const {
        cglib::vec2<int> minPos = getGridIndex(labelInfo.cullRecord.bounds.min);
        cglib::vec2<int> maxPos = getGridIndex(labelInfo.cullRecord.bounds.max);
        bool hasFoundTheSame = false;
        for (int y = minPos(1); y <= maxPos(1); y++) {
            for (int x = minPos(0); x <= maxPos(0); x++) {
                for (const LabelInfo& otherLabelInfo : _recordGrid[y][x]) {
                    if (otherLabelInfo.cullRecord.bounds.inside(labelInfo.cullRecord.bounds)) {
                        if ((!labelInfo.label->allowOverlapSameFeatureId() || !otherLabelInfo.label->allowOverlapSameFeatureId() ||  labelInfo.label->getLocalId() != otherLabelInfo.label->getLocalId()) && testPolygonOverlap(otherLabelInfo.cullRecord.envelope, labelInfo.cullRecord.envelope, 0)) {
                            return false;
                        }
                    }
                    if (labelInfo.label->getLocalId() == otherLabelInfo.label->getLocalId()) {
                        hasFoundTheSame = true;
                    }
                }
            }
        }
        return (!labelInfo.label->sameFeatureIdDependent() || hasFoundTheSame);
    }

    bool LabelCuller::calculateScreenEnvelope(const std::shared_ptr<Label>& label, std::array<cglib::vec2<float>, 4>& envelope) const {
        std::array<cglib::vec3<float>, 4> worldEnvelope;
        if (!label->calculateEnvelope((label->getStyle()->sizeFunc)(_viewState), EXTRA_LABEL_BUFFER, _viewState, worldEnvelope)) {
            return false;
        }
        
        for (std::size_t i = 0; i < 4; i++) {
            cglib::vec2<float> p = cglib::proj_o(cglib::transform_point(worldEnvelope[i], _localCameraProjMatrix));
            envelope[i] = cglib::vec2<float>((p(0) * 0.5f + 0.5f) * _viewState.resolution * _viewState.aspect, (p(1) * 0.5f + 0.5f) * _viewState.resolution);
        }
        return true;
    }

    void LabelCuller::performClustering(std::vector<LabelInfo>& validLabelList, std::mutex& labelMutex) {
        if (validLabelList.empty()) {
            return;
        }

        // Group labels by clustering configuration (clusterEnabled and clusterDistance)
        std::unordered_map<std::string, std::vector<std::size_t>> clusterGroups;
        
        for (std::size_t i = 0; i < validLabelList.size(); i++) {
            const std::shared_ptr<Label>& label = validLabelList[i].label;
            if (label->isClusterEnabled() && label->getClusterDistance() > 0) {
                // Group by layer index and cluster distance to cluster similar labels together
                std::string key = std::to_string(validLabelList[i].layerIndex) + "_" + std::to_string(label->getClusterDistance());
                clusterGroups[key].push_back(i);
            }
        }

        // Process each cluster group independently
        for (auto& entry : clusterGroups) {
            const std::vector<std::size_t>& clusterableIndices = entry.second;
            
            if (clusterableIndices.size() < 2) {
                continue; // Need at least 2 labels to cluster
            }

            // Build clusters using proximity-based grouping
            std::vector<ClusterNode> clusters = buildClusters(clusterableIndices, validLabelList);

            // Process clusters - hide clustered labels and set count on representatives
            for (const ClusterNode& cluster : clusters) {
                if (cluster.count > 1) {
                    // Find the label with highest priority in this cluster to be representative
                    std::size_t representativeIdx = cluster.labelIndices[0];
                    float maxPriority = validLabelList[representativeIdx].priority;
                    
                    for (std::size_t idx : cluster.labelIndices) {
                        if (validLabelList[idx].priority > maxPriority) {
                            maxPriority = validLabelList[idx].priority;
                            representativeIdx = idx;
                        }
                    }

                    // Set cluster count on representative label
                    {
                        std::lock_guard<std::mutex> labelLock(labelMutex);
                        validLabelList[representativeIdx].label->setClusterCount(cluster.count);
                    }

                    // Hide other labels in the cluster by marking them as invalid
                    for (std::size_t idx : cluster.labelIndices) {
                        if (idx != representativeIdx) {
                            validLabelList[idx].valid = false;
                        }
                    }
                }
            }
        }
    }

    std::vector<LabelCuller::ClusterNode> LabelCuller::buildClusters(const std::vector<std::size_t>& clusterableIndices, const std::vector<LabelInfo>& validLabelList) {
        // Create initial singleton clusters
        std::vector<ClusterNode> clusters;
        clusters.reserve(clusterableIndices.size());
        
        for (std::size_t idx : clusterableIndices) {
            ClusterNode node;
            node.labelIndices.push_back(idx);
            node.center = validLabelList[idx].screenPos;
            node.count = 1;
            node.maxDistance = 0.0f;
            clusters.push_back(node);
        }

        if (clusters.empty()) {
            return clusters;
        }

        // Get cluster distance from first label (all in group have same distance)
        float clusterDistancePx = validLabelList[clusterableIndices[0]].label->getClusterDistance();

        // Merge clusters iteratively until no more merging is possible
        bool merged = true;
        while (merged && clusters.size() > 1) {
            merged = (mergeClusters(clusters, clusterDistancePx) > 0);
        }

        return clusters;
    }

    int LabelCuller::mergeClusters(std::vector<ClusterNode>& clusters, float minDistance) {
        int mergeCount = 0;
        
        // Find pairs of clusters that are close enough to merge
        std::vector<std::pair<std::size_t, std::size_t>> mergePairs;
        
        for (std::size_t i = 0; i < clusters.size(); i++) {
            for (std::size_t j = i + 1; j < clusters.size(); j++) {
                cglib::vec2<float> delta = clusters[i].center - clusters[j].center;
                float distance = std::sqrt(delta(0) * delta(0) + delta(1) * delta(1));
                
                if (distance <= minDistance) {
                    mergePairs.push_back({i, j});
                }
            }
        }

        if (mergePairs.empty()) {
            return 0;
        }

        // Merge clusters (process from back to front to maintain indices)
        std::vector<bool> merged(clusters.size(), false);
        std::vector<ClusterNode> newClusters;
        
        for (auto& pair : mergePairs) {
            std::size_t i = pair.first;
            std::size_t j = pair.second;
            
            if (merged[i] || merged[j]) {
                continue;
            }

            // Merge cluster j into cluster i
            ClusterNode& cluster1 = clusters[i];
            const ClusterNode& cluster2 = clusters[j];
            
            // Combine label indices
            cluster1.labelIndices.insert(cluster1.labelIndices.end(), 
                                        cluster2.labelIndices.begin(), 
                                        cluster2.labelIndices.end());
            
            // Update center (weighted average)
            cluster1.center = (cluster1.center * static_cast<float>(cluster1.count) + 
                              cluster2.center * static_cast<float>(cluster2.count)) / 
                             static_cast<float>(cluster1.count + cluster2.count);
            
            // Update count
            cluster1.count += cluster2.count;
            
            // Update max distance
            cluster1.maxDistance = std::max(cluster1.maxDistance, cluster2.maxDistance);
            cglib::vec2<float> delta = cluster1.center - cluster2.center;
            cluster1.maxDistance = std::max(cluster1.maxDistance, std::sqrt(delta(0) * delta(0) + delta(1) * delta(1)));
            
            merged[j] = true;
            mergeCount++;
        }

        // Keep only non-merged clusters
        newClusters.clear();
        for (std::size_t i = 0; i < clusters.size(); i++) {
            if (!merged[i]) {
                newClusters.push_back(clusters[i]);
            }
        }
        
        clusters = std::move(newClusters);
        return mergeCount;
    }
}
