echo "installing fftw"
sudo apt-get -y install libfftw3-3
echo "installing gppiod"
sudo apt-get -y install libgpiod2
echo "installing pulseaudio"
sudo apt-get -y install libpulse0
sudo apt-get -y install libpulse-mainloop-glib0
echo "removing old versions of pihpsdr"
sudo rm -rf /usr/local/bin/pihpsdr
echo "creating start script"
cat <<EOT > start_pihpsdr.sh
cd `pwd`
/usr/local/bin/pihpsdr >log 2>&1
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
echo "copying udev rules"
sudo cp 90-ozy.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
echo "installing pihpsdr"
sudo cp pihpsdr /usr/local/bin
echo "installing shared libraries"
sudo cp libwdsp.so /usr/local/lib
cd /usr/local/lib
sudo ldconfig
if test -f "/boot/config.txt"; then
  if grep -q "gpio=4-13,16-27=ip,pu" /boot/config.txt; then
    echo "/boot/config.txt already contains gpio setup."
  else
    echo "/boot/config.txt does not contain gpio setup - adding it."
    echo "Please reboot system for this to take effect."
    cat <<EGPIO | sudo tee -a /boot/config.txt > /dev/null
[all]
# setup GPIO for pihpsdr controllers
gpio=4-13,16-27=ip,pu
EGPIO
  fi
fi
