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
cp pihpsdr.dshesktop ~/Desktop
if [ ! -d "~/.local" ]; then
  mkdir ~/.local
fi
if [ ! -d "~/.local/share" ]; then
mkdir ~/.local/share
fi
if [ ! -d "~/.local/share/applications" ]; then
mkdir ~/.local/share/applications
fi
cp pihpsdr.desktop ~/.local/share/applications
echo "removing old versions of shared libraries"
sudo rm -rf /usr/local/lib/libwdsp.so
sudo rm -rf /usr/local/lib/libcodec2.*
echo "installing pihpsdr"
sudo cp pihpsdr /usr/local/bin
echo "installing shared libraries"
sudo cp libwdsp.so /usr/local/lib
sudo cp libcodec2.so.0.7 /usr/local/lib
cd /usr/local/lib; sudo ln -s libcodec2.so.0.7 libcodec2.so
sudo ldconfig
