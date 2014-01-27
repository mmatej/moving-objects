#include "comparator.hpp"

void Comparator::setCloud1(pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud) {
    cloud1 = cloud;
}

void Comparator::setCloud2(pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud) {
    cloud2 = cloud;
}

pcl::PointCloud<pcl::PointXYZRGBA>::Ptr Comparator::findMovedPoints() {
    pcl::PointCloud<pcl::PointXYZRGBA>::Ptr movedCloud(new pcl::PointCloud<pcl::PointXYZRGBA>);
    pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cleanedCloud1(removeLargePlanes(cloud1));
    pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cleanedCloud2(removeLargePlanes(cloud2));


    for(pcl::PointCloud<pcl::PointXYZRGBA>::iterator it = cleanedCloud1->begin(), it2 = cleanedCloud2->begin(); it != cleanedCloud1->end(); it++, it2++) {
        if((!pcl::isFinite(*it) && pcl::isFinite(*it2)) || pcl::geometry::distance(it->getVector3fMap(), it2->getVector3fMap()) > distanceThreshold) {
            movedCloud->push_back(*it2);
        }
    }

    return movedCloud;

}

pcl::PointCloud<pcl::PointXYZRGBA>::Ptr Comparator::removeLargePlanes(pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud) {

    pcl::PointCloud<pcl::PointXYZRGBA>::Ptr resultCloud (cloud->makeShared());

    pcl::SACSegmentation<pcl::PointXYZRGBA> segmentation;
    pcl::PointIndices::Ptr inliers (new pcl::PointIndices);
    pcl::ModelCoefficients::Ptr coefficients(new pcl::ModelCoefficients);

    segmentation.setOptimizeCoefficients(true);
    segmentation.setModelType(pcl::SACMODEL_PLANE);
    segmentation.setMethodType(pcl::SAC_RANSAC);
    segmentation.setMaxIterations(100);
    segmentation.setDistanceThreshold(0.02);

    int pointsCount = resultCloud->points.size();

    while(resultCloud->points.size() > 0.3 * pointsCount) {
        segmentation.setInputCloud(resultCloud);
        segmentation.segment(*inliers, *coefficients);
        if(inliers->indices.size() <= 50000) {
            break;
        }

        pcl::ExtractIndices<pcl::PointXYZRGBA> extract;
        extract.setInputCloud(resultCloud);
        extract.setIndices(inliers);
        extract.setNegative(true);
        extract.filterDirectly(resultCloud);
    }

    return resultCloud;
}

pcl::PointCloud<pcl::PointXYZRGBA>::Ptr Comparator::removeOutliers(pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud) {
    pcl::PointCloud<pcl::PointXYZRGBA>::Ptr resultCloud (new pcl::PointCloud<pcl::PointXYZRGBA>);

    pcl::StatisticalOutlierRemoval<pcl::PointXYZRGBA> sor;
    sor.setInputCloud(cloud);
    sor.setMeanK(50);
    sor.setStddevMulThresh(1.0);
    sor.filter(*resultCloud);

    return resultCloud;

}

std::vector<pcl::PointCloud<pcl::PointXYZRGBA>::Ptr > Comparator::extractClusters(pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud) {
    pcl::search::KdTree<pcl::PointXYZRGBA>::Ptr kdTree (new pcl::search::KdTree<pcl::PointXYZRGBA>);
    kdTree->setInputCloud(cloud);

    std::vector<pcl::PointIndices> indices;
    pcl::EuclideanClusterExtraction<pcl::PointXYZRGBA> ece;
    ece.setClusterTolerance(0.02);
    ece.setMinClusterSize(1000);
    ece.setSearchMethod(kdTree);
    ece.setInputCloud(cloud);
    ece.extract(indices);

    std::vector<pcl::PointCloud<pcl::PointXYZRGBA>::Ptr > clusters;
    for(std::vector<pcl::PointIndices>::iterator it = indices.begin(); it != indices.end(); it++) {
        pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cluster(new pcl::PointCloud<pcl::PointXYZRGBA>);
        pcl::ExtractIndices<pcl::PointXYZRGBA> extract;
        extract.setInputCloud(cloud);

        pcl::PointIndices::Ptr pi(new pcl::PointIndices);
        pi->indices=it->indices;
        extract.setIndices(pi);
        extract.setNegative(false);
        extract.filter(*cluster);
        clusters.push_back(cluster);
    }

    return clusters;
}
