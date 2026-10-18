// Excerpt: same function, VID/PID device configurations (lines 685-690).
  local_e40._0_4_ = 0x700fd9;
  LWT::LewittDeviceConf::LewittDeviceConf((LewittDeviceConf *)&DAT_008fb400,&local_e40,1);
  ___cxa_atexit(LWT::LewittDeviceConf::~LewittDeviceConf,&DAT_008fb400,0);
  local_e40 = (char *)CONCAT44(local_e40._4_4_,0x710fd9);
  LWT::LewittDeviceConf::LewittDeviceConf((LewittDeviceConf *)&DAT_008fb418,&local_e40,1);
  ___cxa_atexit(LWT::LewittDeviceConf::~LewittDeviceConf,&DAT_008fb418,0);
