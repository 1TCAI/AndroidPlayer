#include <jni.h>
#include <string>
#include "DerryPlayer.h"
#include "log4c.h" // TODO 第三节课新增
#include "JNICallbakcHelper.h"
#include <android/native_window_jni.h> // ANativeWindow 用来渲染画面的 == Surface对象

extern "C"{
    #include <libavutil/avutil.h>
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_derry_player_MainActivity_getFFmpegVersion(
        JNIEnv *env,
        jobject /* this */) {
    std::string info = "FFmpeg的版本号是:";
    info.append(av_version_info());
    return env->NewStringUTF(info.c_str());
}

DerryPlayer *player = nullptr;
JavaVM *vm = nullptr;
ANativeWindow *window = nullptr;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // 静态初始化 锁
//!什么是静态初始化锁  定义锁的时候直接给了个宏值，编译时刻进行初始化，
//可是为什么要使用静态初始化锁？很可能只是为了简化代码。
//老师解释说  播放器全部学完后，就明白了，后面应该会解释的

jint JNI_OnLoad(JavaVM * vm, void * args) { //跨线程回调
    ::vm = vm;
    return JNI_VERSION_1_6;
}

// 函数指针 实现  渲染工作
void renderFrame(uint8_t * src_data, int width, int height, int src_lineSize) {
    pthread_mutex_lock(&mutex);
    if (!window) {
        pthread_mutex_unlock(&mutex); // 出现了问题后，必须考虑到，释放锁，怕出现死锁问题
    }

    // 设置窗口的大小，各个属性
    ANativeWindow_setBuffersGeometry(window, width, height, WINDOW_FORMAT_RGBA_8888);

    // 他自己有个缓冲区 buffer
    ANativeWindow_Buffer window_buffer; // 目前他是指针吗？

    // 如果我在渲染的时候，是被锁住的，那我就无法渲染，我需要释放 ，防止出现死锁
    if (ANativeWindow_lock(window, &window_buffer, 0)) {
        ANativeWindow_release(window);
        window = 0;

        pthread_mutex_unlock(&mutex); // 解锁，怕出现死锁
        return;
    }

    // TODO 开始真正渲染，因为window没有被锁住了，就可以把 rgba数据 ---> 字节对齐 渲染
    // 填充window_buffer  画面就出来了  === [目标]
    uint8_t *dst_data = static_cast<uint8_t *>(window_buffer.bits);
    int dst_linesize = window_buffer.stride * 4;

    for (int i = 0; i < window_buffer.height; ++i) { // 图：一行一行显示 [高度不用管，用循环了，遍历高度]
        // 视频分辨率：426 * 240
        // 视频分辨率：宽 426
        // 426 * 4(rgba8888) = 1704
        // memcpy(dst_data + i * 1704, src_data + i * 1704, 1704); // 花屏
        // 花屏原因：1704 无法 64字节对齐，所以花屏

        // ANativeWindow_Buffer 64字节对齐的算法，  1704无法以64位字节对齐
        // memcpy(dst_data + i * 1792, src_data + i * 1704, 1792); // OK的
        // memcpy(dst_data + i * 1793, src_data + i * 1704, 1793); // 部分花屏，无法64字节对齐
        //! memcpy(dst_data + i * 1728, src_data + i * 1704, 1728); // 花屏
        //1728是可以被64整除的 但还是花屏 是因为ANativeWindow_Buffer 内部每行还额外需要一个占位

        // ANativeWindow_Buffer 64字节对齐的算法  1728
        // 占位 占位 占位 占位 占位 占位 占位 占位
        // 数据 数据 数据 数据 数据 数据 数据 空值

        // ANativeWindow_Buffer 64字节对齐的算法  1792  空间换时间
        // 占位 占位 占位 占位 占位 占位 占位 占位 占位
        // 数据 数据 数据 数据 数据 数据 数据 空值 空值

        // FFmpeg为什么认为  1704 没有问题 ？
        // FFmpeg是默认采用8字节对齐的，他就认为没有问题， 但是ANativeWindow_Buffer他是64字节对齐的，就有问题

        // 通用的           保证字节对齐
        memcpy(dst_data + i * dst_linesize, src_data + i * src_lineSize, dst_linesize); // OK的
    }//这种不会被企业认可，效率太低。真实情况使用google的libyuv进行转换，显卡语言是公认效率最高的。

    // 数据刷新
    ANativeWindow_unlockAndPost(window); // 解锁后 并且刷新 window_buffer的数据显示画面

    pthread_mutex_unlock(&mutex);
}


//MainActivity的onResume是由主线程在调用的，所以这个是主线程。但是一些解封装操作是耗时的 所以使用子线程
extern "C"
JNIEXPORT void JNICALL          
Java_com_derry_player_DerryPlayer_prepareNative(JNIEnv *env, jobject job, jstring data_source) {
    const char * data_source_ = env->GetStringUTFChars(data_source, 0);
    auto *helper = new JNICallbakcHelper(vm, env, job); // C++子线程回调 ， C++主线程回调
    //基本都是子线程使用JNICallbakcHelper回调，注意跨线程回调
    //第三个参数 job 是要回调给哪个对象 肯定是java层的 jobject
    //但是jobject 不能跨越线程和函数 必须全局引用
    //env不能跨线程 为什么我感觉env也在跨线程 不是子线程回调吗？虽然传入是由主线程传入
    //所以JNICallbakcHelper里面搞了个全新的env
    //如果在构造函数里构造新的env 还说的过去 毕竟是主线程 但看着都是在子线程中构造新的env 
    //!更新 这里传入env其实是考虑了回调可能会有主线程在回调，这种概率很小
    player = new DerryPlayer(data_source_, helper); //这里传入的字符指针很快就会释放了，类内部需要深拷贝
    player->setRenderCallback(renderFrame);
    //这里设置了两层回调，先设置给player，player再设置给 videochannel ，最后由videochannel实际调用回调函数，回调函数进而执行jni层的渲染工作。
    //好像说把 surfaceview对象通过指针一层一层传递下去也可以，但是很难组织起代码。
    player->prepare();//在这个地方启用子线程 让出主线程继续工作
    env->ReleaseStringUTFChars(data_source, data_source_);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_derry_player_DerryPlayer_startNative(JNIEnv *env, jobject thiz) {
    if (player) {
        player->start();    //这里进来的还是主线程
    }

    // 给代码下毒，制作bug
    /*char * p = nullptr;
    char c = *p;*/
}

extern "C"
JNIEXPORT void JNICALL
Java_com_derry_player_DerryPlayer_stopNative(JNIEnv *env, jobject thiz) {
    if (player) {
        player->stop();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_derry_player_DerryPlayer_releaseNative(JNIEnv *env, jobject thiz) {
    pthread_mutex_lock(&mutex);

    // 先释放之前的显示窗口
    if (window) {
        ANativeWindow_release(window);
        window = nullptr;
    }

    pthread_mutex_unlock(&mutex);

    // 释放工作
    DELETE(player);
    DELETE(vm);
    DELETE(window);
}

// 实例化出 window
extern "C"
JNIEXPORT void JNICALL
Java_com_derry_player_DerryPlayer_setSurfaceNative(JNIEnv *env, jobject thiz, jobject surface) {
    pthread_mutex_lock(&mutex);

    // 先释放之前的显示窗口
    if (window) {
        ANativeWindow_release(window);
        window = nullptr;
    }

    // 创建新的窗口用于视频显示
    window = ANativeWindow_fromSurface(env, surface);

    pthread_mutex_unlock(&mutex);
}

// TODO 第七节课增加 获取总时长
extern "C"
JNIEXPORT jint JNICALL
Java_com_derry_player_DerryPlayer_getDurationNative(JNIEnv *env, jobject thiz) {
    if (player) {
        return player->getDuration();
    }
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_derry_player_DerryPlayer_seekNative(JNIEnv *env, jobject thiz, jint play_value) {
    if (player) {
        player->seek(play_value);
    }
}