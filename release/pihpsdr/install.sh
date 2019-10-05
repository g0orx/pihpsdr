echo "installing fftw"
sudo apt-get -y install libfftw3-3
echo "removing old versions of pihpsdr"
sudo rm -rf /usr/local/bin/pihpsdr
echo "creating start script"
cat <<EOT > start_pihpsdr.sh
cd `pwd`
sudo /usr/local/bin/pihpsdr
EOT
chmod +x start_pihpsdr.sh
echo "creating desktop shortcut"
cat <<EOT > pihpsdr.desktop
#!/usr/bin/env xdg-open
[Desktop Entry]
Version=1.0
Type=Application
Terminal=false
Name[eb_GB]=piHPSDR
Exec=`pwd`/start_pihpsdr.sh
Icon=`pwd`/hpsdr_icon.png
Name=piHPSDR
EOT
cp pihpsdr.desktop ~/Desktop
if [ ! -d ~/.local ]; then
  mkdir ~/.local
fi
if [ ! -d ~/.local/share ]; then
mkdir ~/.local/share
fi
if [ ! -d ~/.local/share/applications ]; then
mkdir ~/.local/share/applications
fi
cp pihpsdr.desktop ~/.local/share/applications
echo "removing old versions of shared libraries"
sudo rm -rf /usr/local/lib/libwdsp.so
echo "installing pihpsdr"
sudo cp pihpsdr /usr/local/bin
echo "installing shared libraries"
sudo cp libwdsp.so /usr/local/lib
sudo cp libLimeSuite.so.19.04.1 /usr/local/lib
sudo cp libSoapySDR.so.0.8.0 /usr/local/lib
sudo cp -R SoapySDR /usr/local/lib
cd /usr/local/lib
sudo ln -s libLimeSuite.so.19.04.1 libLimeSuite.so.19.04-1
sudo ln -s libLimeSuite.so.19.04-1 libLimeSuite.so
sudo ln -s libSoapySDR.so.0.8.0 libSoapySDR.so.0.8
sudo ln -s libSoapySDR.so.0.8 libLimeSuite.so
sudo apt-get install librtlsdr-dev
sudo ldconfig
