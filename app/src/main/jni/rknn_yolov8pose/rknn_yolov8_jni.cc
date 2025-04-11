#include <android/log.h>
#include <android/bitmap.h>

#include <jni.h>

#include <sys/time.h>
#include <string>
#include <vector>
#include "yolov8-pose.h"
#include "postprocess.h"
#include "utils/image_drawing.h"
#include "bytetrack/BYTETracker.h"
#include "3rdparty/opencv/opencv2/core/types.hpp"

// extern 使用全局变量
extern int skeleton[38];

extern "C" {

static rknn_app_context_t rknn_app_ctx;

static bool use_zero_copy;
std::vector<Object> trackobj;
std::vector<STrack> output_stracks;
static  BYTETracker tracker(30,30);

JNIEXPORT jint
JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGD("JNI_OnLoad");

    return JNI_VERSION_1_4;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
    LOGD("JNI_OnUnload");

}

JNIEXPORT jboolean JNICALL
Java_com_sq_rknn_rknnyolov8posewithbytetrack_YoloV8PoseDetect_init(JNIEnv *env, jobject thiz, jstring jmodel_path,
                                                jstring jlabel_list_path, jboolean juse_zero_copy) {
    int ret;
    const char *modelPath = (env->GetStringUTFChars(jmodel_path, 0));
    const char *labelListPath = (env->GetStringUTFChars(jlabel_list_path, 0));

    memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));

    use_zero_copy = juse_zero_copy;

    init_post_process(labelListPath);

    ret = use_zero_copy ? init_yolov8_pose_model(modelPath, &rknn_app_ctx) :
          init_yolov8_pose_model(modelPath, &rknn_app_ctx);
    if (ret != 0) {
        LOGE("init_yolov8_model fail! ret=%d model_path=%s\n", ret, modelPath);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

void decobj_to_trackobj(object_detect_result_list &objects, std::vector<Object> &trackobj)
{
    // 如果objects不为空，初始化trackobj
    if (objects.count > 0)
    {
        trackobj.clear();
    }
    // 将objects转换为trackobj
    for (int i = 0; i < objects.count; i++) {
        object_detect_result *det_result = &(objects.results[i]);
        // 新建Object对象
        Object trackobj_temp;
        trackobj_temp.classId = det_result->cls_id;
        trackobj_temp.score = det_result->prop;
        trackobj_temp.box.x = det_result->box.left;
        trackobj_temp.box.y = det_result->box.top;
        trackobj_temp.box.width = det_result->box.right - det_result->box.left;
        trackobj_temp.box.height = det_result->box.bottom - det_result->box.top;
        trackobj.push_back(trackobj_temp);
    }
}

JNIEXPORT jboolean JNICALL
Java_com_sq_rknn_rknnyolov8posewithbytetrack_YoloV8PoseDetect_detect(JNIEnv *env, jobject thiz, jobject jbitmap) {
    AndroidBitmapInfo dstInfo;

    if (ANDROID_BITMAP_RESULT_SUCCESS != AndroidBitmap_getInfo(env, jbitmap, &dstInfo)) {
        LOGE("get bitmap info failed");
        return JNI_FALSE;
    }

    void *dstBuf;
    if (ANDROID_BITMAP_RESULT_SUCCESS != AndroidBitmap_lockPixels(env, jbitmap, &dstBuf)) {
        LOGE("lock dst bitmap failed");
        return JNI_FALSE;
    }

    int ret;
    image_buffer_t src_image;
    memset(&src_image, 0, sizeof(image_buffer_t));

    src_image.width = dstInfo.width;
    src_image.height = dstInfo.height;
    src_image.format = IMAGE_FORMAT_RGBA8888;
    src_image.virt_addr = static_cast<unsigned char *>(dstBuf);
    src_image.size = dstInfo.width * dstInfo.height * 4;

//    LOGI("width=%d; height=%d; stride=%d; format=%d;flag=%d",
//         dstInfo.width, //  width=2700 (900*3)
//         dstInfo.height, // height=2025 (675*3)
//         dstInfo.stride, // stride=10800 (2700*4)
//         dstInfo.format, // format=1 (ANDROID_BITMAP_FORMAT_RGBA_8888=1)
//         dstInfo.flags); // flags=0 (ANDROID_BITMAP_RESULT_SUCCESS=0)

    object_detect_result_list od_results;
//    for (int i = 0; i < 10; ++i) {
    int64_t start_us = getCurrentTimeUs();

    ret = use_zero_copy ?
          inference_yolov8_pose_model(&rknn_app_ctx, &src_image, &od_results)
                        : inference_yolov8_pose_model(&rknn_app_ctx, &src_image, &od_results);

    int64_t elapse_us = getCurrentTimeUs() - start_us;
//    LOGI("Total Elapse Time = %.2fms, FPS = %.2f\n", elapse_us / 1000.f,
//         1000.f * 1000.f / elapse_us);
//    }

    if (ret != 0) {
        LOGE("inference_yolov8_model fail! ret=%d\n", ret);
        return JNI_FALSE;
    }

    // 画框和概率
    char text[256];
    for (int i = 0; i < od_results.count; i++) {
        object_detect_result *det_result = &(od_results.results[i]);
        LOGI("%s @ (%d %d %d %d) %.3f\n", coco_cls_to_name(det_result->cls_id),
             det_result->box.left, det_result->box.top,
             det_result->box.right, det_result->box.bottom,
             det_result->prop);
        int x1 = det_result->box.left;
        int y1 = det_result->box.top;
        int x2 = det_result->box.right;
        int y2 = det_result->box.bottom;

        draw_rectangle(&src_image, x1, y1, x2 - x1, y2 - y1, COLOR_BLUE, 3);

        sprintf(text, "%s %.1f%%", coco_cls_to_name(det_result->cls_id), det_result->prop * 100);
        draw_text(&src_image, text, x1, y1 - 20, COLOR_RED, 10);
        for (int j = 0; j < 38/2; ++j)
        {
            draw_line(&src_image, (int)(det_result->keypoints[skeleton[2*j]-1][0]),(int)(det_result->keypoints[skeleton[2*j]-1][1]),
                      (int)(det_result->keypoints[skeleton[2*j+1]-1][0]),(int)(det_result->keypoints[skeleton[2*j+1]-1][1]),COLOR_ORANGE,3);
        }

        for (int j = 0; j < 17; ++j)
        {
            draw_circle(&src_image, (int)(det_result->keypoints[j][0]),(int)(det_result->keypoints[j][1]),1, COLOR_YELLOW,1);
        }
    }

    AndroidBitmap_unlockPixels(env, jbitmap);

    return JNI_TRUE;
}

JNIEXPORT jobject JNICALL
Java_com_sq_rknn_rknnyolov8posewithbytetrack_YoloV8PoseDetect_detect2(JNIEnv *env, jobject thiz, jobject jbitmap) {
    AndroidBitmapInfo dstInfo;

    if (ANDROID_BITMAP_RESULT_SUCCESS != AndroidBitmap_getInfo(env, jbitmap, &dstInfo)) {
        LOGE("get bitmap info failed");
        return NULL;
    }

    void *dstBuf;
    if (ANDROID_BITMAP_RESULT_SUCCESS != AndroidBitmap_lockPixels(env, jbitmap, &dstBuf)) {
        LOGE("lock dst bitmap failed");
        return NULL;
    }

    int ret;
    image_buffer_t src_image;
    memset(&src_image, 0, sizeof(image_buffer_t));

    src_image.width = dstInfo.width;
    src_image.height = dstInfo.height;
    src_image.format = IMAGE_FORMAT_RGBA8888;
    src_image.virt_addr = static_cast<unsigned char *>(dstBuf);
    src_image.size = dstInfo.width * dstInfo.height * 4;

    object_detect_result_list od_results;
    int64_t start_us = getCurrentTimeUs();

    ret = use_zero_copy ?
          inference_yolov8_pose_model(&rknn_app_ctx, &src_image, &od_results)
                        : inference_yolov8_pose_model(&rknn_app_ctx, &src_image, &od_results);

    int64_t elapse_us = getCurrentTimeUs() - start_us;

    if (ret != 0) {
        LOGE("inference_yolov8_model fail! ret=%d\n", ret);
        AndroidBitmap_unlockPixels(env, jbitmap);
        return NULL;
    }

    // 将检测对象转换成追踪对象
    decobj_to_trackobj(od_results, trackobj);
    output_stracks = tracker.update(trackobj);

    char text[256];
    for (unsigned long i = 0; i < output_stracks.size(); i++)
    {
        std::vector<float> tlwh = output_stracks[i].tlwh;
        bool vertical = tlwh[2] / tlwh[3] > 1.6;
        if (tlwh[2] * tlwh[3] > 20 && !vertical)
        {
            LOGI("%d @ (%f %f %f %f) %.3f\n", output_stracks[i].track_id,
                 tlwh[0], tlwh[1], tlwh[2]+tlwh[0], tlwh[3]+tlwh[1],
                 output_stracks[i].score);
            cv::Scalar s = tracker.get_color(output_stracks[i].track_id);
            // 提取蓝色、绿色、红色分量（假设s是BGR顺序）
            auto b = static_cast<unsigned char>(s[0]);
            auto g = static_cast<unsigned char>(s[1]);
            auto r = static_cast<unsigned char>(s[2]);

            // 组合成0xAARRGGBB格式的unsigned int（这里alpha=0xFF不透明）
            unsigned int color = 0xFF000000 | (r << 16) | (g << 8) | b;

            draw_rectangle(&src_image, tlwh[0], tlwh[1], tlwh[2], tlwh[3], color, 3);

            sprintf(text, "%d %s %.1f%%", output_stracks[i].track_id, coco_cls_to_name(output_stracks[i].cls_id), output_stracks[i].score * 100);
            draw_text(&src_image, text, tlwh[0], tlwh[1] - 20, COLOR_RED, 10);
        }
    }
    // 画框和概率
//    for (int i = 0; i < od_results.count; i++) {
//        object_detect_result *det_result = &(od_results.results[i]);
//        LOGI("%s @ (%d %d %d %d) %.3f\n", coco_cls_to_name(det_result->cls_id),
//             det_result->box.left, det_result->box.top,
//             det_result->box.right, det_result->box.bottom,
//             det_result->prop);
//        int x1 = det_result->box.left;
//        int y1 = det_result->box.top;
//        int x2 = det_result->box.right;
//        int y2 = det_result->box.bottom;
//
//        draw_rectangle(&src_image, x1, y1, x2 - x1, y2 - y1, COLOR_BLUE, 3);
//
//        sprintf(text, "%s %.1f%%", coco_cls_to_name(det_result->cls_id), det_result->prop * 100);
//        draw_text(&src_image, text, x1, y1 - 20, COLOR_RED, 10);
//
//        for (int j = 0; j < 38/2; ++j)
//        {
//            draw_line(&src_image, (int)(det_result->keypoints[skeleton[2*j]-1][0]),(int)(det_result->keypoints[skeleton[2*j]-1][1]),
//                      (int)(det_result->keypoints[skeleton[2*j+1]-1][0]),(int)(det_result->keypoints[skeleton[2*j+1]-1][1]),COLOR_ORANGE,3);
//        }
//
//        for (int j = 0; j < 17; ++j)
//        {
//            draw_circle(&src_image, (int)(det_result->keypoints[j][0]),(int)(det_result->keypoints[j][1]),1, COLOR_YELLOW,1);
//        }
//    }

    AndroidBitmap_unlockPixels(env, jbitmap);

    // 获取 Java 类和构造函数
    jclass detectionResultClass = env->FindClass("com/sq/rknn/rknnyolov8posewithbytetrack/DetectionResult");
    jclass objectResultClass = env->FindClass("com/sq/rknn/rknnyolov8posewithbytetrack/DetectionResult$ObjectResult");

    jmethodID detectionResultConstructor = env->GetMethodID(detectionResultClass, "<init>", "()V");
    jmethodID objectResultConstructor = env->GetMethodID(objectResultClass, "<init>", "(FFFFIF)V");

    jmethodID addMethod = env->GetMethodID(detectionResultClass, "addObjectResult", "(Lcom/sq/rknn/rknnyolov8posewithbytetrack/DetectionResult$ObjectResult;)V");

    // 创建 DetectionResult 实例
    jobject detectionResult = env->NewObject(detectionResultClass, detectionResultConstructor);

    // 填充检测结果
    for (int i = 0; i < od_results.count; i++) {
        object_detect_result result = od_results.results[i];
        jobject objectResult = env->NewObject(objectResultClass, objectResultConstructor,
                                              (float)result.box.left, (float)result.box.top, (float)result.box.right, (float)result.box.bottom,
                                                 result.cls_id, result.prop);
        env->CallVoidMethod(detectionResult, addMethod, objectResult);
    }

    return detectionResult;
}

JNIEXPORT jboolean JNICALL
Java_com_sq_rknn_rknnyolov8posewithbytetrack_YoloV8PoseDetect_release(JNIEnv *env, jobject thiz) {
    deinit_post_process();

    int ret = use_zero_copy ? release_yolov8_pose_model(&rknn_app_ctx)
                            : release_yolov8_pose_model(&rknn_app_ctx);
    if (ret != 0) {
        LOGE("release_yolov8_model fail! ret=%d\n", ret);
        return false;
    }

    return true;
}

}