sudo apt-get -y install libfftw3-3
sudo apt-get -y install xscreensaver
echo "removing old shared libraries"
sudo rm -rf /usr/local/lib/libwdsp.so
sudo rm -rf /usr/local/lib/libpsk.so
sudo rm -rf /usr/local/lib/libcodec2.*
sudo rm -rf /usr/local/lib/libSoapySDR.*
echo "installing shared libraries"
sudo cp libwdsp.so /usr/local/lib
sudo cp libpsk.so /usr/local/lib
sudo cp libcodec2.so.0.5 /usr/local/lib
sudo cp libSoapySDR.so.0.5-1 /usr/local/lib
cd /usr/local/lib; sudo ln -s libcodec2.so.0.5 libcodec2.so
cd /usr/local/lib; sudo ln -s libSoapySDR.so.0.5-1 libSoapySDR.so.0.5.0
cd /usr/local/lib; sudo ln -s libSoapySDR.so.0.5-1 libSoapySDR.so
sudo ldconfig
echo "setting up for USB OZY device"
sudo cp 90-ozy.rules /etc/udev/rules.d
sudo udevadm control --reload-rules
echo "creating desktop icon"
cp pihpsdr.desktop ~/Desktop
cp pihpsdr.desktop ~/.local/share/applications
