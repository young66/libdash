## 2016-06-20 | Mengbai Xiao
----------
* To build libdash and the player on ubuntu, follow the instructions

  libdash.so
  1. sudo apt-get install git-core build-essential cmake libxml2-dev libcurl4-openssl-dev
  2. git clone git@git.corp.adobe.com:menxiao/libdash.git
  3. cd libdash/libdash
  4. mkdir build
  5. cd build
  6. cmake ../
  7. make
  8. cd bin
  9. The library and a simple test of the network part of the library should be available now. You can test the network part of the library with
  10. ./libdash_networpart_test

  QTSamplePlayer
  Prerequisite: libdash must be built as described in the previous section.

  1. sudo apt-add-repository ppa:ubuntu-sdk-team/ppa (ppa:canonical-qt5-edgers/qt5-proper is not required anymore)
  2. sudo apt-get update
  3. sudo apt-get install qtmultimedia5-dev qtbase5-dev libqt5widgets5 libqt5core5a libqt5gui5 libqt5multimedia5 libqt5multimediawidgets5 libqt5opengl5 libav-tools libavcodec-dev libavdevice-dev libavfilter-dev libavformat-dev libavutil-dev libpostproc-dev libswscale-dev
  4. cd libdash/libdash/qtsampleplayer
  5. export LIBAVROOT="${PATH_TO_LIBDASH_REPO}/libdash/libav"
  6. mkdir build
  7. cd build
  8. cmake ../
  9. make
  10. ./qtsampleplayer

* libdash does not support HTTP/2 by default.

* Video quality adaptation logic is located in the player code.