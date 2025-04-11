package com.sq.rknn.rknnyolov8posewithbytetrack;

import android.graphics.Bitmap;

public class YoloV8PoseDetect {
    static {
        System.loadLibrary("rknn_yolov8pose");
    }

    public native boolean init(String modelPath, String labelListPath, boolean useZeroCopy);

    public native boolean detect(Bitmap srtBitmap);
    public native Object detect2(Bitmap srtBitmap);

    public native boolean release();
}
