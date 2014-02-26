#include <iostream>
#include <sstream>
#include <string>
#include <pcl/io/pcd_io.h>
#include <pcl/visualization/pcl_visualizer.h>

#include "moving_objects_identificator.hpp"

#include <pcl/pcl_macros.h>
#include <pcl/apps/3d_rec_framework/pipeline/global_nn_classifier.h>
#include <pcl/apps/3d_rec_framework/pc_source/mesh_source.h>
#include <pcl/apps/3d_rec_framework/feature_wrapper/global/esf_estimator.h>
#include <pcl/apps/3d_rec_framework/utils/metrics.h>
#include <pcl/console/parse.h>

using namespace std;

pcl::PointCloud<pcl::PointXYZ>::Ptr cloud1 (new pcl::PointCloud<pcl::PointXYZ>);
pcl::PointCloud<pcl::PointXYZ>::Ptr cloud2 (new pcl::PointCloud<pcl::PointXYZ>);

MovingObjectsIdentificator moi(0.01f);

int main(int argc, char* argv[]) {

    if(argc < 3) {
        PCL_ERROR("run as ./project /path/to/cloud1/ /path/to/cloud2/ [pcd format]");
        return -1;
    }

    string fCloud1;
    string fCloud2;
    string models;
    string training;
    int NN;

    pcl::console::parse_argument(argc, argv, "-cloud1", fCloud1);
    pcl::console::parse_argument(argc, argv, "-cloud2", fCloud2);
    pcl::console::parse_argument(argc, argv, "-models", models);
    pcl::console::parse_argument(argc, argv, "-training", training);
    pcl::console::parse_argument(argc, argv, "-nn", NN);
    cout << "arg1 - " << fCloud1 << endl;
    cout << "arg2 - " << fCloud2 << endl;

    if(pcl::io::loadPCDFile<pcl::PointXYZ>(fCloud1, *cloud1) == -1) {
        PCL_ERROR("Cloud1 reading failed\n");
        return(-1);
    }

    if(pcl::io::loadPCDFile<pcl::PointXYZ>(fCloud2, *cloud2) == -1) {
        PCL_ERROR("Cloud2 reading failed\n");
        return(-1);
    }

    std::vector<int> indices;

    pcl::removeNaNFromPointCloud(*cloud1, *cloud1, indices);
    pcl::removeNaNFromPointCloud(*cloud2, *cloud2, indices);

    cout << "finding moved objects" << endl;

    moi.setInputCloud1(cloud1);
    moi.setInputCloud2(cloud2);

    moi.findDifference();
    moi.removeOutliers();
    std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> clusters = moi.extractClusters();
    cout << "clusters: " << clusters.size() << endl;
//
    boost::shared_ptr<pcl::rec_3d_framework::MeshSource<pcl::PointXYZ> > mesh_source (new pcl::rec_3d_framework::MeshSource<pcl::PointXYZ>);
    mesh_source->setPath(models);
    mesh_source->setResolution(150);
    mesh_source->setTesselationLevel(1);
    mesh_source->setViewAngle(57.f);
    mesh_source->setRadiusSphere(1.5f);
    mesh_source->setModelScale(1.f);
    mesh_source->generate(training);

    boost::shared_ptr<pcl::rec_3d_framework::Source<pcl::PointXYZ> > cast_source;
    cast_source = boost::static_pointer_cast<pcl::rec_3d_framework::MeshSource<pcl::PointXYZ> > (mesh_source);

    boost::shared_ptr<pcl::rec_3d_framework::PreProcessorAndNormalEstimator<pcl::PointXYZ, pcl::Normal> > normal_estimator;
    normal_estimator.reset (new pcl::rec_3d_framework::PreProcessorAndNormalEstimator<pcl::PointXYZ, pcl::Normal>);
    normal_estimator->setCMR (true);
    normal_estimator->setDoVoxelGrid(true);
    normal_estimator->setRemoveOutliers(true);
    normal_estimator->setFactorsForCMR(3, 7);

    //esf -> compare with (c)vfh
    boost::shared_ptr<pcl::rec_3d_framework::ESFEstimation<pcl::PointXYZ, pcl::ESFSignature640> > estimator;
    estimator.reset(new pcl::rec_3d_framework::ESFEstimation<pcl::PointXYZ, pcl::ESFSignature640>);

    boost::shared_ptr<pcl::rec_3d_framework::GlobalEstimator<pcl::PointXYZ, pcl::ESFSignature640> > cast_estimator;
    cast_estimator = boost::dynamic_pointer_cast<pcl::rec_3d_framework::ESFEstimation<pcl::PointXYZ, pcl::ESFSignature640> > (estimator);

    pcl::rec_3d_framework::GlobalNNPipeline<flann::L1, pcl::PointXYZ, pcl::ESFSignature640> global;
    global.setDataSource(cast_source);
    global.setTrainingDir(training);
    string dsc = "esf";
    global.setDescriptorName(dsc);
    global.setFeatureEstimator(cast_estimator);
    global.setNN(NN);
    global.initialize(false);

    vector<string> categories;

    pcl::visualization::PCLVisualizer viewer ("Result");
    viewer.setBackgroundColor(0, 0, 0);
    viewer.addPointCloud<pcl::PointXYZ> (cloud2, "current frame");
    int clusterNum = 0;
    float dist_ = 0.03f;
    int categoryTextId = 0;
    for(vector<pcl::PointCloud<pcl::PointXYZ>::Ptr>::iterator it = clusters.begin(); it != clusters.end(); it++) {
        global.setInputCloud(*it);
        global.classify();

        vector<string> clusterCategories;
        vector<float> confidence;

        global.getCategory(clusterCategories);
        global.getConfidence(confidence);
        categories.push_back(clusterCategories[0]);

        pcl::visualization::PointCloudColorHandlerRandom<pcl::PointXYZ> randomColor (*it);
        viewer.addPointCloud<pcl::PointXYZ> (*it, randomColor, "cluster" + clusterNum);
        clusterNum++;

        Eigen::Vector4f centroid;
        pcl::compute3DCentroid (**it, centroid);

        for(int i = 0; i < clusterCategories.size(); i++) {

            pcl::PointXYZ textPosition;
            textPosition.x = centroid[0];
            textPosition.y = centroid[1] - static_cast<float> (i+1) * dist_;
            textPosition.z = centroid[2];

            ostringstream prob_str;
            prob_str.precision (1);
            prob_str << clusterCategories[i] << " [" <<confidence[i] << "]";

            stringstream textId;
            textId << "text" << categoryTextId;

            viewer.addText3D(prob_str.str(), textPosition, 0.015f, 1, 0, 1, textId.str(), 0);
            categoryTextId++;
        }
    }



    viewer.initCameraParameters();

    while(!viewer.wasStopped()) {
        viewer.spinOnce(100);
        boost::this_thread::sleep (boost::posix_time::milliseconds(100));
    }

    return 0;
}
