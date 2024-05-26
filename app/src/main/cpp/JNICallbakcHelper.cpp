#include "JNICallbakcHelper.h"

JNICallbakcHelper::JNICallbakcHelper(JavaVM *vm, JNIEnv *env, jobject job) {
    this->vm = vm;
    this->env = env;
    // this->job = job; // 坑： jobject不能跨越线程，不能跨越函数，必须全局引用
    this->job = env->NewGlobalRef(job); // 提升全局引用

    jclass clazz = env->GetObjectClass(job);
    jmd_prepared = env->GetMethodID(clazz, "onPrepared", "()V");
    jmd_onError = env->GetMethodID(clazz, "onError", "(I)V");// TODO 第二节课新增

    // 播放音频的时间搓回调
    jmd_onProgress = env->GetMethodID(clazz, "onProgress", "(I)V");// TODO 第七节课新增 2.1
}

JNICallbakcHelper::~JNICallbakcHelper() {
    vm = 0;
    env->DeleteGlobalRef(job);
    job = 0;
    env = 0;
}

void JNICallbakcHelper::onPrepared(int thread_mode) {
    if (thread_mode == THREAD_MAIN) {
        // 主线程：直接调用即可
        env->CallVoidMethod(job, jmd_prepared);
    } else if (thread_mode == THREAD_CHILD) {
        // 子线程 env也不可以跨线程吧 对的   全新的env
        JNIEnv * env_child;
        vm->AttachCurrentThread(&env_child, 0);
        env_child->CallVoidMethod(job, jmd_prepared);   // jmd_prepared 就是 onPrepared
        vm->DetachCurrentThread();
    }
}

/* TODO 第二节课新增 --- start */
void JNICallbakcHelper::onError(int thread_mode, int error_code) {
    if (thread_mode == THREAD_MAIN) {
        //主线程
        env->CallVoidMethod(job, jmd_onError);
    } else {
        //子线程
        //当前子线程的 JNIEnv
        JNIEnv *env_child;
        vm->AttachCurrentThread(&env_child, 0);
        env_child->CallVoidMethod(job, jmd_onError, error_code);
        vm->DetachCurrentThread();
    }
}

/* TODO 第二节课新增 --- end */

// TODO 第七节课增加 2.1
void JNICallbakcHelper::onProgress(int thread_mode, int audio_time) {
    if (thread_mode == THREAD_MAIN) {
        //主线程
        env->CallVoidMethod(job, jmd_onError);
    } else {
        //子线程
        //当前子线程的 JNIEnv
        JNIEnv *env_child;
        vm->AttachCurrentThread(&env_child, 0);
        env_child->CallVoidMethod(job, jmd_onProgress, audio_time);
        vm->DetachCurrentThread();
    }
}


