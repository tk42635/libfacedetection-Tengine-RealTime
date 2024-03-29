/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * License); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * AS IS BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*
 * Copyright (c) 2019, Open AI Lab
 * Author: chunyinglv@openailab.com
 */

#include <iostream>
#include <iomanip>
#include <sys/time.h>
#include <dirent.h>

#include "tengine_c_api.h"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

using namespace std;


float show_threshold = 0.5;

struct Box
{
    float x0;
    float y0;
    float x1;
    float y1;
    int class_idx;
    float score;
};

cv::Mat post_process_ssd(cv::Mat& img, float threshold, float* outdata, int num)
{
    std::vector<Box> boxes;
    int line_width = img.cols * 0.005;
    /*printf("--------------------------------------------\n");
    printf("Face id: prob%%\tBOX:( x0 , y0 ),( x1 , y1 )\n");
    printf("--------------------------------------------\n");*/
    int detected_face_num = 0;
    cv::Mat result = img.clone();
    for(int i = 0; i < num; i++)
    {
        if(outdata[1] >= threshold)
        {
            detected_face_num += 1;
            Box box;
            box.class_idx = outdata[0];
            box.score = outdata[1];
            box.x0 = outdata[2] * img.cols;
            box.y0 = outdata[3] * img.rows;
            box.x1 = outdata[4] * img.cols;
            box.y1 = outdata[5] * img.rows;
            boxes.push_back(box);
            printf("Face %d:\t%.0f%%\t", detected_face_num, box.score * 100);
            printf("BOX:( %g , %g ),( %g , %g )\n", box.x0, box.y0, box.x1, box.y1);
            
            
        }
        outdata += 6;
    }
    printf("detect faces : %d \n", detected_face_num);
    for(int i = 0; i < ( int )boxes.size(); i++)
    {
        Box box = boxes[i];
        cv::rectangle(img, cv::Rect(box.x0, box.y0, (box.x1 - box.x0), (box.y1 - box.y0)), cv::Scalar(255, 255, 0),
                      line_width);

        std::ostringstream score_str;
        score_str.precision(3);
        score_str << box.score;
        std::string label = score_str.str();
        int baseLine = 0;
        cv::Size label_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.3, 1, &baseLine);
        cv::rectangle(img,
                      cv::Rect(cv::Point(box.x0, box.y0 - label_size.height),
                               cv::Size(label_size.width, label_size.height + baseLine)),
                      cv::Scalar(255, 255, 0), CV_FILLED);
        cv::putText(img, label, cv::Point(box.x0, box.y0), cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(0, 0, 0));
    }
    
    return img;
    /*std::cout << "======================================\n";
    std::cout << "[DETECTED IMAGE SAVED]:\t" << save_name << "\n";
    std::cout << "======================================\n";*/
}

void get_input_data(cv::Mat& img, float* input_data, int img_h, int img_w)
{
    int mean[3] = { 104,117,123 };
    unsigned char* src_ptr=(unsigned char*)(img.ptr(0));
    int hw = img_h * img_w;
    for(int h = 0; h < img_h; h++)
    {
        for(int w = 0; w < img_w; w++)
        {
            for(int c = 0; c < 3; c++)
            {
                input_data[c * hw + h * img_w + w] =(float)(*src_ptr - mean[c]);
                src_ptr++;
            }
        }
    }
}

void get_image_names(std::string file_path, std::vector<std::string>& file_names)
{
	DIR *dir;
    struct dirent *ptr;
    dir = opendir(file_path.c_str());
    while( (ptr = readdir(dir)) != NULL)
    {
        string filename = string(ptr->d_name);
        if (filename == "." || filename == ".."){
            continue;
        }
        string path = file_path + string("/") + filename;
        file_names.push_back(path);
    }
    closedir(dir);
    sort(file_names.begin(), file_names.end());
}

int main(int argc, char* argv[])
{
    /*if(argc < 5)
    {
        std::cout << "[Usage]: " << argv[0] << " <proto> <caffemodel> <pic_address> \n";
        return 0;
    }*/
    std::string proto_name_ = argv[1];
    std::string mdl_name_ = argv[2];
    //std::string image_file_path = argv[3];
    int repeat_count = 1;
    
    
    vector<string> file_names;
    size_t count=0;
    
    cv::VideoCapture cap(0);
    cap.set(CV_CAP_PROP_FRAME_WIDTH, 320);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 240);
    cv::Mat img;
    string video_name;

    //读取本地视频文件
    //cin>>video_name;    
    //cap.open("/home/firefly/Downloads/" + video_name);

    //读取本地图片库    
	//get_image_names(image_file_path, file_names);
    
    //std::string save_file = "save.jpg";
 

    init_tengine();
    if(request_tengine_version("0.9") < 0)
        return 1;
	double s = (double)cv::getTickCount();
    
    while(1)
    {
		
    double t = (double)cv::getTickCount();
    
    graph_t graph = create_graph(nullptr, "caffe", proto_name_.c_str(), mdl_name_.c_str());
    if(graph == nullptr)
    {
        std::cout << "Create graph0 failed\n";
        std::cout << "errno: " << get_tengine_errno() << "\n";
        return -1;
    }
    

    /* get input tensor */
    cap >> img;
    if(img.empty())
    {
        std::cerr << "failed to read image file " <<  "\n";
        return -1;
    }
#if 0
    // resize to 320 x 240
    cv::Mat resize_img;
    int img_w = 320;
    int img_h = 240;
    cv::resize(img, resize_img, cv::Size(img_w, img_h), 0, 0,cv::INTER_NEAREST);
   
    float* input_data = ( float* )malloc(sizeof(float) * img_h * img_w * 3);
  
    get_input_data(resize_img, input_data, img_h, img_w);
#else
    // use origin image size
    int img_h = img.rows;
    int img_w = img.cols;
    float* input_data = ( float* )malloc(sizeof(float) * img_h * img_w * 3);
    get_input_data(img, input_data, img_h, img_w);
#endif
    int node_idx = 0;
    int tensor_idx = 0;
    tensor_t input_tensor = get_graph_input_tensor(graph, node_idx, tensor_idx);
    int dims[] = {1, 3, img_h, img_w};
    set_tensor_shape(input_tensor, dims, 4);
    /* setup input buffer */
    if(set_tensor_buffer(input_tensor, input_data, 3 * img_h * img_w * 4) < 0)
    {
        std::printf("Set buffer for tensor failed\n");
        return -1;
    }

    prerun_graph(graph);

    // time run_graph
    
    const char* repeat = std::getenv("REPEAT_COUNT");
    if(repeat)
        repeat_count = std::strtoul(repeat, NULL, 10);

    struct timeval t0, t1;
    float avg_time = 0.f;
    gettimeofday(&t0, NULL);
    for(int i = 0; i < repeat_count; i++)
        run_graph(graph, 1);
    gettimeofday(&t1, NULL);
    float mytime = ( float )((t1.tv_sec * 1000000 + t1.tv_usec) - (t0.tv_sec * 1000000 + t0.tv_usec)) / 1000;
    avg_time += mytime;
    std::cout << "--------------------------------------\n";
    std::cout << "repeat " << repeat_count << " times, avg time per run is " << avg_time / repeat_count << " ms\n";
    
   


    // post process
    tensor_t out_tensor = get_graph_output_tensor(graph, 0, 0);    //"detection_out");
    int out_dim[4];
    get_tensor_shape(out_tensor, out_dim, 4);
    float* outdata = ( float* )get_tensor_buffer(out_tensor);
    int num = out_dim[1];
    cv::Mat result = post_process_ssd(img, show_threshold, outdata, num);

    

    // free
    release_graph_tensor(out_tensor);
    release_graph_tensor(input_tensor);
    postrun_graph(graph);
    destroy_graph(graph);
    free(input_data);

    cv::waitKey(10);

    t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
    double fps = 1.0 / t;

    cv::putText(result, "FPS: " + to_string(fps), cv::Point(5, 20), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255));
    cv::imshow("result", result);   

	cout << count <<" Time consumed: "<< t << "s" << "   FPS: "<< fps<<endl;

    count++;
}

s = ((double)cv::getTickCount() - s) / cv::getTickFrequency();

cout<< "Average FPS: " << (count+1)/s <<endl;
    

release_tengine();

    return 0;
}
