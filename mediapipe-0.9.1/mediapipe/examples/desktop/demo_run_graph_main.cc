// Copyright 2019 The MediaPipe Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// An example of sending OpenCV webcam frames into a MediaPipe graph.
#include <cstdlib>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/framework/port/file_helpers.h"
#include "mediapipe/framework/port/opencv_highgui_inc.h"
#include "mediapipe/framework/port/opencv_imgproc_inc.h"
#include "mediapipe/framework/port/opencv_video_inc.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"

#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/calculators/util/landmarks_to_render_data_calculator.pb.h"

#include "mediapipe/framework/formats/classification.pb.h"

#include <WinSock2.h> //windows
#pragma comment(lib, "ws2_32.lib")

constexpr char kInputStream[] = "input_video";
constexpr char kOutputStream[] = "output_video";
constexpr char kWindowName[] = "MediaPipe";

constexpr char kOutputLandmarks[] = "output_landmarks";

constexpr char kOutputHandednesses[] = "output_handedness";

ABSL_FLAG(std::string, calculator_graph_config_file, "",
          "Name of file containing text format CalculatorGraphConfig proto.");
ABSL_FLAG(std::string, input_video_path, "",
          "Full path of video to load. "
          "If not provided, attempt to use a webcam.");
ABSL_FLAG(std::string, output_video_path, "",
          "Full path of where to save result (.mp4 only). "
          "If not provided, show result in a window.");

absl::Status RunMPPGraph() {

    int sock;
    struct sockaddr_in addr;
    WSAData wsaData;
    struct timeval tv;

    float H0_x, H0_y, H0_z;
    float H1_x, H1_y, H1_z;

    WSACleanup();
    WSAStartup(MAKEWORD(2, 0), &wsaData);   //MAKEWORD(2, 0)はwinsockのバージョン2.0ってこと
    sock = socket(AF_INET, SOCK_DGRAM, 0);  //AF_INETはIPv4、SOCK_DGRAMはUDP通信、0は？

    sock = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(50008);// 待ち受けポート番号を50008にする
    addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.2");// 送信アドレスを設定

    std::string calculator_graph_config_contents;
    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(
        absl::GetFlag(FLAGS_calculator_graph_config_file),
        &calculator_graph_config_contents));
    LOG(INFO) << "Get calculator graph config contents: "
            << calculator_graph_config_contents;
    mediapipe::CalculatorGraphConfig config =
        mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(
            calculator_graph_config_contents);

    LOG(INFO) << "Initialize the calculator graph.";
    mediapipe::CalculatorGraph graph;
    MP_RETURN_IF_ERROR(graph.Initialize(config));

    LOG(INFO) << "Initialize the camera or load the video.";
    cv::VideoCapture capture;
    const bool load_video = !absl::GetFlag(FLAGS_input_video_path).empty();
    if (load_video) {
        capture.open(absl::GetFlag(FLAGS_input_video_path));
    } else {
        capture.open(0);
    }
    RET_CHECK(capture.isOpened());

    cv::VideoWriter writer;
    const bool save_video = !absl::GetFlag(FLAGS_output_video_path).empty();
    if (!save_video) {
        cv::namedWindow(kWindowName, /*flags=WINDOW_AUTOSIZE*/ 1);
    #if (CV_MAJOR_VERSION >= 3) && (CV_MINOR_VERSION >= 2)
        capture.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        capture.set(cv::CAP_PROP_FPS, 30);
    #endif
    }

    LOG(INFO) << "Start running the calculator graph.";
    ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller poller,
                    graph.AddOutputStreamPoller(kOutputStream));
    ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller poller_landmarks,
                    graph.AddOutputStreamPoller(kOutputLandmarks));
    ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller presence_poller,
                    graph.AddOutputStreamPoller("landmark_presence"));
    ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller poller_handednesses,
                    graph.AddOutputStreamPoller(kOutputHandednesses));
    MP_RETURN_IF_ERROR(graph.StartRun({}));

    LOG(INFO) << "Start grabbing and processing frames.";
    bool grab_frames = true;
    while (grab_frames) {
        Sleep(1000 / 1000);
        // Capture opencv camera or video frame.
        cv::Mat camera_frame_raw;
        capture >> camera_frame_raw;
        if (camera_frame_raw.empty()) {
            if (!load_video) {
            LOG(INFO) << "Ignore empty frames from camera.";
            continue;
            }
            LOG(INFO) << "Empty frame, end of video reached.";
            break;
        }
        cv::Mat camera_frame;
        cv::cvtColor(camera_frame_raw, camera_frame, cv::COLOR_BGR2RGB);
        if (!load_video) {
            cv::flip(camera_frame, camera_frame, /*flipcode=HORIZONTAL*/ 1);
        }

        // Wrap Mat into an ImageFrame.
        auto input_frame = absl::make_unique<mediapipe::ImageFrame>(
            mediapipe::ImageFormat::SRGB, camera_frame.cols, camera_frame.rows,
            mediapipe::ImageFrame::kDefaultAlignmentBoundary);
        cv::Mat input_frame_mat = mediapipe::formats::MatView(input_frame.get());
        camera_frame.copyTo(input_frame_mat);

        // Send image packet into the graph.
        size_t frame_timestamp_us =
            (double)cv::getTickCount() / (double)cv::getTickFrequency() * 1e6;
        MP_RETURN_IF_ERROR(graph.AddPacketToInputStream(
            kInputStream, mediapipe::Adopt(input_frame.release())
                                .At(mediapipe::Timestamp(frame_timestamp_us))));

        // Get the graph result packet, or stop if that fails.
        mediapipe::Packet packet;
        mediapipe::Packet packet_landmarks;
        mediapipe::Packet presence_packet;
        mediapipe::Packet packet_handednesses;
    
        std::cout << "HERE 0" << std::endl;

        if (!poller.Next(&packet)) break;
        std::cout << "HERE 1" << std::endl;
        auto& output_frame = packet.Get<mediapipe::ImageFrame>();

        if (!presence_poller.Next(&presence_packet)) break;
        auto is_landmark_present = presence_packet.Get<bool>();
        if (is_landmark_present) 
            if (poller_landmarks.Next(&packet_landmarks) && poller_handednesses.Next(&packet_handednesses))
            {
                auto& output_handedness = packet_handednesses.Get<std::vector<mediapipe::ClassificationList, std::allocator<mediapipe::ClassificationList>>>();
                auto& output_landmarks = packet_landmarks.Get<std::vector<mediapipe::NormalizedLandmarkList>>();
                //output_landmarks[0].PrintDebugString();
                if (output_landmarks.size() == output_handedness.size()) {
                    for (int i = 0; i < output_handedness.size(); i++) {
                        if (output_handedness[i].classification()[0].label()[0] == 'R') {
                            H0_x = output_landmarks[i].landmark(0).x();
                            H0_y = output_landmarks[i].landmark(0).y();
                            H0_z = output_landmarks[i].landmark(0).z();
                            for (int j = 0; j < output_landmarks[i].landmark_size(); j++){
                                printf("[%2d]{\n    x:%f\n    y:%f\n    z:%f\n}\n",
                                    j,
                                    output_landmarks[i].landmark(j).x(),
                                    output_landmarks[i].landmark(j).y(),
                                    output_landmarks[i].landmark(j).z()
                                );
                            }
                        }
                        if (output_handedness[i].classification()[0].label()[0] == 'L') {
                            H1_x = output_landmarks[i].landmark(0).x();
                            H1_y = output_landmarks[i].landmark(0).y();
                            H1_z = output_landmarks[i].landmark(0).z();
                        }
                    }
                }
            }
        char buff[512];
        sprintf_s(buff, "%lf %lf %lf %lf %lf %lf", H0_x, H0_y, H0_z, H1_x, H1_y, H1_z);
        std::cout << buff << std::endl;
        sendto(sock, buff, sizeof(buff), 0, (struct sockaddr*)&addr, sizeof(addr));
       
        std::cout << "HERE 2" << std::endl;

        // Convert back to opencv for display or saving.
        cv::Mat output_frame_mat = mediapipe::formats::MatView(&output_frame);
        cv::cvtColor(output_frame_mat, output_frame_mat, cv::COLOR_RGB2BGR);
        if (save_video) {
            if (!writer.isOpened()) {
            LOG(INFO) << "Prepare video writer.";
            writer.open(absl::GetFlag(FLAGS_output_video_path),
                        mediapipe::fourcc('a', 'v', 'c', '1'),  // .mp4
                        capture.get(cv::CAP_PROP_FPS), output_frame_mat.size());
            RET_CHECK(writer.isOpened());
            }
            writer.write(output_frame_mat);
        } else {
            cv::imshow(kWindowName, output_frame_mat);
            // Press any key to exit.
            const int pressed_key = cv::waitKey(5);
            if (pressed_key >= 0 && pressed_key != 255) grab_frames = false;
        }
    }

    LOG(INFO) << "Shutting down.";
    if (writer.isOpened()) writer.release();
    MP_RETURN_IF_ERROR(graph.CloseInputStream(kInputStream));
    return graph.WaitUntilDone();
}

int main(int argc, char** argv) {
    google::InitGoogleLogging(argv[0]);
    absl::ParseCommandLine(argc, argv);
    absl::Status run_status = RunMPPGraph();
    if (!run_status.ok()) {
        LOG(ERROR) << "Failed to run the graph: " << run_status.message();
        return EXIT_FAILURE;
    } else {
        LOG(INFO) << "Success!";
    }
    return EXIT_SUCCESS;
}
