#include <cstdlib>
#include <array>
#include <iostream>
#include <ostream>


#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>


#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/framework/formats/detection.pb.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/formats/rect.pb.h"

#include "mediapipe/framework/port/file_helpers.h"
#include "mediapipe/framework/port/opencv_highgui_inc.h"
#include "mediapipe/framework/port/opencv_imgproc_inc.h"
#include "mediapipe/framework/port/opencv_video_inc.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/gpu/gl_calculator_helper.h"
#include "mediapipe/gpu/gpu_buffer.h"
#include "mediapipe/gpu/gpu_shared_data_internal.h"
#include "mediapipe/util/resource_util.h"

constexpr char kInputStream[] = "input_video";
constexpr char kOutputStream[] = "output_video";
constexpr char kOutputLandmarks[] = "hand_landmarks";
constexpr char kLandmarkPresence[] = "landmark_presence";


constexpr char kWindowName[] = "HandTrackingTest";

#define NUM_FLOATS 60

float landmarkData[NUM_FLOATS];
struct sockaddr_in serverAddress;
int clientSocket = socket(AF_INET, SOCK_DGRAM, 0);

ABSL_FLAG(std::string, calculator_graph_config_file, "",
          "Name of file containing text format CalculatorGraphConfig proto.");
ABSL_FLAG(std::string, input_video_path, "",
          "Full path of video to load. "
          "If not provided, attempt to use a webcam.");
ABSL_FLAG(std::string, output_video_path, "",
          "Full path of where to save result (.mp4 only). "
          "If not provided, show result in a window.");

absl::Status RunMPPGraph() {

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

  LOG(INFO) << "Initialize the GPU.";
  ASSIGN_OR_RETURN(auto gpu_resources, mediapipe::GpuResources::Create());
  MP_RETURN_IF_ERROR(graph.SetGpuResources(std::move(gpu_resources)));
  mediapipe::GlCalculatorHelper gpu_helper;
  gpu_helper.InitializeForTest(graph.GetGpuResources().get());

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

  ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller poller_presence,
                   graph.AddOutputStreamPoller(kLandmarkPresence));

  MP_RETURN_IF_ERROR(graph.StartRun({}));

  LOG(INFO) << "Start grabbing and processing frames.";
  bool grab_frames = true;
  while (grab_frames) {
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
    cv::cvtColor(camera_frame_raw, camera_frame, cv::COLOR_BGR2RGBA);
    if (!load_video) {
      cv::flip(camera_frame, camera_frame, /*flipcode=HORIZONTAL*/ 1);
    }

    // Wrap Mat into an ImageFrame.
    auto input_frame = absl::make_unique<mediapipe::ImageFrame>(
        mediapipe::ImageFormat::SRGBA, camera_frame.cols, camera_frame.rows,
        mediapipe::ImageFrame::kGlDefaultAlignmentBoundary);
    cv::Mat input_frame_mat = mediapipe::formats::MatView(input_frame.get());
    camera_frame.copyTo(input_frame_mat);

    // Prepare and add graph input packet.
    size_t frame_timestamp_us = (double)cv::getTickCount() / (double)cv::getTickFrequency() * 1e6;
    MP_RETURN_IF_ERROR(
        gpu_helper.RunInGlContext([&input_frame, &frame_timestamp_us, &graph,
                                   &gpu_helper]() -> absl::Status {
          // Convert ImageFrame to GpuBuffer.
          auto texture = gpu_helper.CreateSourceTexture(*input_frame.get());
          auto gpu_frame = texture.GetFrame<mediapipe::GpuBuffer>();
          glFlush();
          texture.Release();
          // Send GPU image packet into the graph.
          MP_RETURN_IF_ERROR(graph.AddPacketToInputStream(
              kInputStream, mediapipe::Adopt(gpu_frame.release())
                                .At(mediapipe::Timestamp(frame_timestamp_us))));
          return absl::OkStatus();
        }));

    // Get the graph result packet, or stop if that fails.
    mediapipe::Packet packet;
    mediapipe::Packet landmarkPacket;
    mediapipe::Packet presencePacket;

    if (!poller.Next(&packet)) break;
    std::unique_ptr<mediapipe::ImageFrame> output_frame;


    if(!poller_presence.Next(&presencePacket)) break;

    auto isLandmarkPresent = presencePacket.Get<bool>();
    

    if(isLandmarkPresent){
      if(!poller_landmarks.Next(&landmarkPacket)) break;

      auto &outputHandLandmarks = landmarkPacket.Get<std::vector<mediapipe::NormalizedLandmarkList>>();



      for(int i = 0; i < outputHandLandmarks.size(); i++){

        //float landmark = outputHandLandmarks[i].landmark()[12].y();


        for(int j = 0; j < outputHandLandmarks[i].landmark().size() * 3; j+=3){

          landmarkData[j] = outputHandLandmarks[i].landmark()[j / 3].x();
          landmarkData[j + 1] = outputHandLandmarks[i].landmark()[j / 3].y();
          landmarkData[j + 2] = outputHandLandmarks[i].landmark()[j / 3].z();

        }

          //send data to the server
        char buf[sizeof(float) * NUM_FLOATS];
        memcpy(buf, &landmarkData, sizeof(buf));

        //LOG(INFO) << landmark;

        sendto(clientSocket, buf, sizeof(buf), 0, (struct sockaddr *)&serverAddress, sizeof(serverAddress));

      }   

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

  char buffer[1024];
  const char* message = "Connected to client...";


  //specify the server address and port
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(8080);
  inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);


  absl::Status run_status = RunMPPGraph();
  if (!run_status.ok()) {
    LOG(ERROR) << "Failed to run the graph: " << run_status.message();
    return EXIT_FAILURE;
  } else {
    LOG(INFO) << "Success!";
    close(clientSocket);
  }

  return EXIT_SUCCESS;
}
