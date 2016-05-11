cp libwdsp.so /usr/local/lib
cp libcodec2.so.0.5 /usr/local/lib
cp libSoapySDR.so.0.5-1 /usr/local/lib
cd /usr/local/lib; ln -s libcodec2.so.0.5 libcodec2.so
cd /usr/local/lib; ln -s libSoapySDR.so.0.5-1 libSoapySDR.so.0.5.0
cd /usr/local/lib; ln -s libSoapySDR.so.0.5-1 libSoapySDR.so
ldconfig
