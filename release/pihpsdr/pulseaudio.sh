sudo usermod -a -G pulse-access pi
sudo usermod -a -G audio root
sudo usermod -a -G pulse root
sudo usermod -a -G pulse-access root
sudo cp pulseaudio.service /etc/systemd/system
sudo systemctl --system enable pulseaudio.service
sudo systemctl --system start pulseaudio.service
