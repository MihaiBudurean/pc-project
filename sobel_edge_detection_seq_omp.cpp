#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include <omp.h>
#include <mpi.h>

// Function to apply Sobel operator and compute the gradient magnitude
void sobel_edge_detection(const cv::Mat& src, cv::Mat& dst) {
    int rows = src.rows;
    int cols = src.cols;

    cv::Mat grad_x, grad_y;
    cv::Sobel(src, grad_x, CV_32F, 1, 0);
    cv::Sobel(src, grad_y, CV_32F, 0, 1);

    dst = cv::Mat(rows, cols, CV_32F);

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            float gx = grad_x.at<float>(y, x);
            float gy = grad_y.at<float>(y, x);
            dst.at<float>(y, x) = std::sqrt(gx * gx + gy * gy);
        }
    }

    dst.convertTo(dst, CV_8U);
}

// Parallel Sobel Edge Detection using OpenMP
void sobel_edge_detection_omp(const cv::Mat& src, cv::Mat& dst) {
    int rows = src.rows;
    int cols = src.cols;

    cv::Mat grad_x, grad_y;
    cv::Sobel(src, grad_x, CV_32F, 1, 0);
    cv::Sobel(src, grad_y, CV_32F, 0, 1);

    dst = cv::Mat(rows, cols, CV_32F);

    #pragma omp parallel for collapse(2)
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            float gx = grad_x.at<float>(y, x);
            float gy = grad_y.at<float>(y, x);
            dst.at<float>(y, x) = std::sqrt(gx * gx + gy * gy);
        }
    }

    dst.convertTo(dst, CV_8U);
}

// Parallel Sobel Edge Detection using MPI
void sobel_edge_detection_mpi(const cv::Mat& src, cv::Mat& dst, int rank, int size) {
    int rows = src.rows;
    int cols = src.cols;
    int local_rows = rows / size;
    int start_row = rank * local_rows;
    int end_row = (rank == size - 1) ? rows : start_row + local_rows;

    cv::Mat local_src = src.rowRange(start_row, end_row);
    cv::Mat grad_x, grad_y;
    cv::Sobel(local_src, grad_x, CV_32F, 1, 0);
    cv::Sobel(local_src, grad_y, CV_32F, 0, 1);

    cv::Mat local_dst(local_rows, cols, CV_32F);

    for (int y = 0; y < local_rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            float gx = grad_x.at<float>(y, x);
            float gy = grad_y.at<float>(y, x);
            local_dst.at<float>(y, x) = std::sqrt(gx * gx + gy * gy);
        }
    }

    local_dst.convertTo(local_dst, CV_8U);

    if (rank == 0) {
        dst = cv::Mat(rows, cols, CV_8U);
    }

    MPI_Gather(local_dst.data, local_rows * cols, MPI_UNSIGNED_CHAR,
               dst.data, local_rows * cols, MPI_UNSIGNED_CHAR,
               0, MPI_COMM_WORLD);
}


int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <image_path>" << std::endl;
        return -1;
    }

    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    std::string image_path = argv[1];
    cv::Mat image = cv::imread(image_path, cv::IMREAD_GRAYSCALE);

    if (image.empty()) {
        std::cerr << "Error: Could not open or find the image." << std::endl;
        return -1;
    }

    cv::Mat edge_image_seq, edge_image_omp;

    auto start_seq = std::chrono::high_resolution_clock::now();
    sobel_edge_detection(image, edge_image_seq);
    auto end_seq = std::chrono::high_resolution_clock::now();

    auto start_omp = std::chrono::high_resolution_clock::now();
    sobel_edge_detection_omp(image, edge_image_omp);
    auto end_omp = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed_seq = end_seq - start_seq;
    std::chrono::duration<double> elapsed_omp = end_omp - start_omp;

    std::cout << "Sequential Sobel Edge Detection Time: " << elapsed_seq.count() << " seconds" << std::endl;
    std::cout << "Parallel Sobel Edge Detection Time (OpenMP): " << elapsed_omp.count() << " seconds" << std::endl;

    cv::imwrite("edge_image_seq.jpg", edge_image_seq);
    cv::imwrite("edge_image_omp.jpg", edge_image_omp);

    return 0;
}