#! /bin/bash

target="/home/user/MySoftware/study_ffmpeg/test_ffmpeg6/test_ffmpeg6/sample-mp4-file.mp4"
name="/home/user/MyBuild/build_test_ffmpeg6/sample.mp4"
target2="/home/user/MySoftware/study_ffmpeg/test_ffmpeg6/test_ffmpeg6/_test4.avi"
name2="/home/user/MyBuild/build_test_ffmpeg6/_test4.avi"
target3="/home/user/MySoftware/study_ffmpeg/test_ffmpeg6/test_ffmpeg6/sample_640x360.mkv"
name3="/home/user/MyBuild/build_test_ffmpeg6/sample.mkv"
target4="/home/user/MySoftware/study_ffmpeg/test_ffmpeg6/test_ffmpeg6/no_video.avi"
name4="/home/user/MyBuild/build_test_ffmpeg6/no_video.avi"
target5="/home/user/MySoftware/study_ffmpeg/test_ffmpeg6/test_ffmpeg6/sample2.mkv"
name5="/home/user/MyBuild/build_test_ffmpeg6/sample2.mkv"
target6="/home/user/MySoftware/study_ffmpeg/test_ffmpeg6/test_ffmpeg6/nightwish.mp4"
name6="/home/user/MyBuild/build_test_ffmpeg6/nightwish.mp4"
target7="/home/user/MySoftware/study_ffmpeg/test_ffmpeg6/test_ffmpeg6/settings/settings.json"
name7="/home/user/MyBuild/build_test_ffmpeg6/settings/settings.json"
# if link is not created

if [ ! -h $name ]
then
    ln -s $target $name
else
	echo "link have already created!"
fi

if [ ! -h $name2 ]
then
	ln -s $target2 $name2
else
	echo "link have already created!"
fi

if [ ! -h $name3 ]
then
	ln -s $target3 $name3
else
	echo "link have already created!"
fi

if [ ! -h $name4 ]
then
	ln -s $target4 $name4
else
	echo "link have already created!"
fi

if [ ! -h $name5 ]
then
	ln -s $target5 $name5
else
	echo "link have already created!"
fi

if [ ! -h $name6 ]
then
        ln -s $target6 $name6
else
        echo "link have already created!"
fi

if [ ! -h $name7 ]
then
        ln -s $target7 $name7
else
        echo "link have already created!"
fi
