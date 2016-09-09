sudo apt-get -y install libfftw3-3
sudo apt-get -y install xscreensaver
sudo rm -rf /usr/local/lib/libwdsp.so
sudo rm -rf /usr/local/lib/libpsk.so
sudo rm -rf /usr/local/lib/libcodec2.so
sudo rm -rf /usr/local/lib/libSoapySDR.so
sudo cp libwdsp.so /usr/local/lib
sudo cp libpsk.so /usr/local/lib
sudo cp libcodec2.so.0.5 /usr/local/lib
sudo cp libSoapySDR.so.0.5-1 /usr/local/lib
sudo cd /usr/local/lib; ln -s libcodec2.so.0.5 libcodec2.so
sudo cd /usr/local/lib; ln -s libSoapySDR.so.0.5-1 libSoapySDR.so.0.5.0
sudo cd /usr/local/lib; ln -s libSoapySDR.so.0.5-1 libSoapySDR.so
sudo ldconfig
