package com.derry.player;

import androidx.appcompat.app.AppCompatActivity;

import android.annotation.SuppressLint;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Environment;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;
import java.io.File;

public class MainActivity extends AppCompatActivity implements SeekBar.OnSeekBarChangeListener {

    private DerryPlayer player;
    private TextView tv_state;
    private SurfaceView surfaceView;

    // TODO 第七节课增加
    // 直播视频是没有拖动条的（直播是没有总时长）（隐藏），  若非直播的视频文件才有拖动条（视频文件是有总时长）（显示）
    private SeekBar seekBar;
    private TextView tv_time; // 显示播放时间
    private boolean isTouch; // 用户是否拖拽了 拖动条，（默认是没有拖动false）
    private int duration; // 获取native层的总时长

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON, WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.activity_main);
        tv_state = findViewById(R.id.tv_state);
        surfaceView = findViewById(R.id.surfaceView);

        // TODO 第七节课增加
        tv_time = findViewById(R.id.tv_time);
        seekBar = findViewById(R.id.seekBar);
        seekBar.setOnSeekBarChangeListener(this);

        player = new DerryPlayer();
        player.setSurfaceView(surfaceView);
        player.setDataSource(
                new File(Environment.getExternalStorageDirectory() + File.separator + "demo.mp4")
                        .getAbsolutePath());

        // TODO 测试播放特殊的视频，会有bug ： 声音是OK的, 视频只能显示第一帧 然后自动回到首页了
        /*player.setDataSource(
                new File(Environment.getExternalStorageDirectory() + File.separator + "chengdu.mp4")
                        .getAbsolutePath());*/

        // TODO 湖南卫视直播地址播放  OK的
        // rtmp://58.200.131.2:1935/livetv/hunantv
        // player.setDataSource("rtmp://58.200.131.2:1935/livetv/hunantv");

        // 还不支持 rtsp
        // rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov
        // player.setDataSource("rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov");

        // https://stream7.iqilu.com/10339/upload_transcode/202002/18/20200218114723HDu3hhxqIT.mp4
        // player.setDataSource("https://stream7.iqilu.com/10339/upload_transcode/202002/18/20200218114723HDu3hhxqIT.mp4");

        // 准备成功的回调处    <----  C++ 子线程调用的
        player.setOnPreparedListener(new DerryPlayer.OnPreparedListener() {//重写接口
            @Override
            public void onPrepared() {

                // TODO 第七节课增加 1.1 拖动条默认隐藏，如果播放视频有总时长，就显示所以拖动条控件
                // 得到视频总时长： 直播：duration=0，  非直播-视频：duration=有值的
                duration = player.getDuration();

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        // TODO 第七节课增加 1.1
                        if (duration != 0) {

                            // duration == 119 转换成  01:59

                            // 非直播-视频
                            // tv_time.setText("00:00/" + "01:59");
                            tv_time.setText("00:00/" + getMinutes(duration) + ":" + getSeconds(duration));
                            tv_time.setVisibility(View.VISIBLE); // 显示
                            seekBar.setVisibility(View.VISIBLE); // 显示
                        }

                        // Toast.makeText(MainActivity.this, "准备成功，即将开始播放", Toast.LENGTH_SHORT).show();
                        tv_state.setTextColor(Color.GREEN); // 绿色
                        tv_state.setText("恭喜init初始化成功");
                    }
                });
                player.start(); // 调用 C++ 开始播放
            }
        });

        // 准备过程中，发送了错误
        /* TODO 第二节课新增 --- start */
        player.setOnErrorListener(new DerryPlayer.OnErrorListener() {
            @Override
            public void onError(final String errorInfo) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        // Toast.makeText(MainActivity.this, "出错了，错误详情是:" + errorInfo, Toast.LENGTH_SHORT).show();
                        tv_state.setTextColor(Color.RED); // 红色
                        tv_state.setText("哎呀,错误啦，错误:" + errorInfo);
                    }
                });
            }
        });
        /* TODO 第二节课新增 --- end */

        player.setOnOnProgressListener(new DerryPlayer.OnProgressListener() {
            @Override
            public void onProgress(final int progress) {

                // TODO C++层吧audio_time时间搓传递上来 --> 被动？
                // 【如果是人为拖动的，不能干预我们计算】 否则会混乱
                if (!isTouch) {

                    // C++层是异步线程调用上来的，小心，UI
                    runOnUiThread(new Runnable() {
                        @SuppressLint("SetTextI18n")
                        @Override
                        public void run() {
                            if (duration != 0) {
                                // TODO 播放信息 动起来
                                // progress:C++层 ffmpeg获取的当前播放【时间（单位是秒 80秒都有，肯定不符合界面的显示） -> 1分20秒】
                                tv_time.setText(getMinutes(progress) + ":" + getSeconds(progress)
                                        + "/" +
                                        getMinutes(duration) + ":" + getSeconds(duration));

                                // TODO 拖动条 动起来 seekBar相对于总时长的百分比
                                // progress == C++层的 音频时间搓  ----> seekBar的百分比
                                // seekBar.setProgress(progress * 100 / duration 以秒计算seekBar相对总时长的百分比);
                                seekBar.setProgress(progress * 100 / duration);
                            }
                        }
                    });
                }
            }
        });
    }

    @Override // ActivityThread.java Handler 主线程在调用这个，所以ndk层的准备函数也是主线程在调用
    protected void onResume() { 
        super.onResume();
        player.prepare();   //触发 player类的准备工作函数 函数又调用NDK 底层的c++准备函数 如果准备成功就会回调函数 返回信息
    }

    @Override
    protected void onStop() {
        super.onStop();
        player.stop();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        player.release();
    }

    // TODO >>>>>>>>>>>>>>>>>>>>>>>>>>>

    // TODO 第七节课增加 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

    // 119 ---> 1.多一点点
    private String getMinutes(int duration) { // 给我一个duration，转换成xxx分钟
        int minutes = duration / 60;
        if (minutes <= 9) {
            return "0" + minutes;
        }
        return "" + minutes;
    }

    // 119 ---> 60 59
    private String getSeconds(int duration) { // 给我一个duration，转换成xxx秒
        int seconds = duration % 60;
        if (seconds <= 9) {
            return "0" + seconds;
        }
        return "" + seconds;
    }

    /**
     * 当前拖动条进度发送了改变 回调此函数
     * @param seekBar 控件
     * @param progress 1~100
     * @param fromUser 是否用户拖拽导致的改变
     */
    @SuppressLint("SetTextI18n")
    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (fromUser) {
            // progress 是进度条的进度 （0 - 100） ------>   秒 分 的效果
            tv_time.setText(getMinutes(progress * duration / 100)
                    + ":" +
                    getSeconds(progress * duration / 100) + "/" +
                    getMinutes(duration) + ":" + getSeconds(duration));
        }
    }

    // 手按下去，回调此函数
    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {
        isTouch = true;
    }

    // TODO 第七节课增加 3
    // 手松开（SeekBar当前值 ---> C++层），回调此函数
    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        isTouch = false;

        int seekBarProgress = seekBar.getProgress(); // 获取当前seekbar当前进度

        // SeekBar1~100  -- 转换 -->  C++播放的时间（61.546565）
        int playProgress = seekBarProgress * duration / 100;

        player.seek(playProgress);
    }
}
