#include "Telescope.hh"

#include <regex>

using namespace std::chrono_literals;

void Layer::fw_start(){
  if(!m_fw) return;

  if(!m_js_conf.HasMember("hotmask")){
    fprintf(stderr, "JSON configure file error: no hotmask section \n");
    throw;
  }
  const auto& js_hotmask =  m_js_conf["hotmask"];
  std::printf("\nMasking Layer %s ", m_name.c_str());
  for(const auto &hot: js_hotmask.GetArray()){
    uint64_t  x = hot[0].GetUint64();
    uint64_t  y = hot[1].GetUint64();
    std::printf(" [%u, %u] ", x, y);
    m_fw->SetPixelRegister(x, y, "MASK_EN", true);
  }
  std::printf("\n ");  
  
  if(!m_js_conf.HasMember("firmware")){
    fprintf(stderr, "JSON configure file error: no firmware section \n");
    throw;
  }
  const auto& js_fw_conf =  m_js_conf["firmware"];
  for(const auto &reg: js_fw_conf.GetObject()){
    m_fw->SetFirmwareRegister(reg.name.GetString(), reg.value.GetUint64());
  }

  if(!m_js_conf.HasMember("sensor")){
    fprintf(stderr, "JSON configure file error: no sensor section \n");
    throw;
  }
  const auto& js_sn_conf =  m_js_conf["sensor"];
  for(const auto &reg: js_sn_conf.GetObject()){
    m_fw->SetAlpideRegister(reg.name.GetString(), reg.value.GetUint64());
  }
  
  m_fw->SetAlpideRegister("CMU_DMU_CONF", 0x70);// token
  m_fw->SetAlpideRegister("CHIP_MODE", 0x3d); //trigger MODE
  m_fw->SendAlpideBroadcast("RORST"); //Readout (RRU/TRU/DMU) reset, commit token
  m_fw->SetFirmwareRegister("FIRMWARE_MODE", 1); //run ext trigger
  std::fprintf(stdout, " fw start %s\n", m_fw->DeviceUrl().c_str());
}

void Layer::fw_stop(){
  if(!m_fw) return;
  m_fw->SetFirmwareRegister("FIRMWARE_MODE", 0); //fw must be stopped before chip
  m_fw->SetAlpideRegister("CHIP_MODE", 0x3c); // configure mode
  std::fprintf(stdout, " fw stop  %s\n", m_fw->DeviceUrl().c_str());
}


void Layer::fw_init(){
  if(!m_fw) return;

  //m_fw->SendFirmwareCommand("RESET");
  
  m_fw->SetFirmwareRegister("TRIG_DELAY", 1); //25ns per dig (FrameDuration?)
  m_fw->SetFirmwareRegister("GAP_INT_TRIG", 20);

  //=========== init part ========================
  // 3.8 Chip initialization
  // GRST
  m_fw->SetFirmwareRegister("FIRMWARE_MODE", 0);
  m_fw->SetFirmwareRegister("ADDR_CHIP_ID", 0x10); //OB
  m_fw->SendAlpideBroadcast("GRST"); // chip global reset
  m_fw->SetAlpideRegister("CHIP_MODE", 0x3c); // configure mode
  // DAC setup
  m_fw->SetAlpideRegister("VRESETP", 0x75); //117
  m_fw->SetAlpideRegister("VRESETD", 0x93); //147
  m_fw->SetAlpideRegister("VCASP", 0x56);   //86
  uint32_t vcasn = 57;
  uint32_t ithr  = 51;
  m_fw->SetAlpideRegister("VCASN", vcasn);   //57 Y50
  m_fw->SetAlpideRegister("VPULSEH", 0xff); //255 
  m_fw->SetAlpideRegister("VPULSEL", 0x0);  //0
  m_fw->SetAlpideRegister("VCASN2",vcasn+12);  //62 Y63  VCASN+12
  m_fw->SetAlpideRegister("VCLIP", 0x0);    //0
  m_fw->SetAlpideRegister("VTEMP", 0x0);
  m_fw->SetAlpideRegister("IAUX2", 0x0);
  m_fw->SetAlpideRegister("IRESET", 0x32);  //50
  m_fw->SetAlpideRegister("IDB", 0x40);     //64
  m_fw->SetAlpideRegister("IBIAS", 0x40);   //64
  m_fw->SetAlpideRegister("ITHR", ithr);   //51  empty 0x32; 0x12 data, not full.  0x33 default, threshold
  // 3.8.1 Configuration of in-pixel logic
  m_fw->SendAlpideBroadcast("PRST");  //pixel matrix reset
  m_fw->SetPixelRegisterFullChip("MASK_EN", 0);
  m_fw->SetPixelRegisterFullChip("PULSE_EN", 0);
  m_fw->SendAlpideBroadcast("PRST");  //pixel matrix reset
  // 3.8.2 Configuration and start-up of the Data Transmission Unit, PLL
  m_fw->SetAlpideRegister("DTU_CONF", 0x008d); // default
  m_fw->SetAlpideRegister("DTU_DAC",  0x0088); // default
  m_fw->SetAlpideRegister("DTU_CONF", 0x0085); // clear pll disable bit
  m_fw->SetAlpideRegister("DTU_CONF", 0x0185); // set pll reset bit
  m_fw->SetAlpideRegister("DTU_CONF", 0x0085); // clear reset bit
  // 3.8.3 Setting up of readout
  // 3.8.3.1a (OB) Setting CMU and DMU Configuration Register
  m_fw->SetAlpideRegister("CMU_DMU_CONF", 0x70); //Token, disable MCH, enable DDR, no previous OB
  m_fw->SetAlpideRegister("TEST_CTRL", 0x400); //Disable Busy Line
  // 3.8.3.2 Setting FROMU Configuration Registers and enabling readout mode
  // FROMU Configuration Register 1,2
  m_fw->SetAlpideRegister("FROMU_CONF_1", 0x00); //Disable external busy, no triger delay
  m_fw->SetAlpideRegister("FROMU_CONF_2", 20); //STROBE duration, alice testbeam 100
  // FROMU Pulsing Register 1,2
  // m_fw->SetAlpideRegister("FROMU_PULSING_2", 0xffff); //yiliu: test pulse duration, max  
  // Periphery Control Register (CHIP MODE)
  // m_fw->SetAlpideRegister("CHIP_MODE", 0x3d); //trigger MODE
  // RORST 
  // m_fw->SendAlpideBroadcast("RORST"); //Readout (RRU/TRU/DMU) reset, commit token
  //===========end of init part =====================

  //user init
  //
  //  

  //
  //end of user init
  
  std::fprintf(stdout, " fw init  %s\n", m_fw->DeviceUrl().c_str());
}

void Layer::rd_start(){
  if(m_is_async_reading){
    std::cout<< "old AsyncReading() has not been stopped "<<std::endl;
    return;
  }

  m_fut_async_rd = std::async(std::launch::async, &Layer::AsyncPushBack, this);
  if(!m_is_async_watching){
    m_fut_async_watch = std::async(std::launch::async, &Layer::AsyncWatchDog, this);
  }
}

void Layer::rd_stop(){
  m_is_async_reading = false;
  if(m_fut_async_rd.valid())
    m_fut_async_rd.get();

  m_is_async_watching = false;
  if(m_fut_async_watch.valid())
    m_fut_async_watch.get();
}

uint64_t Layer::AsyncPushBack(){ // IMPROVE IT AS A RING
  m_vec_ring_ev.clear();
  m_vec_ring_ev.resize(m_size_ring);
  m_count_ring_write = 0;
  m_count_ring_read = 0;
  m_hot_p_read = m_size_ring -1; // tail

  uint32_t tg_expected = 0;
  uint32_t flag_wait_first_event = true;

  m_rd->Open();
  std::fprintf(stdout, " rd start  %s\n", m_rd->DeviceUrl().c_str());
  m_is_async_reading = true;
  while (m_is_async_reading){
    auto df = m_rd? m_rd->Read(1000ms):nullptr; // TODO: read a vector
    if(!df){
      continue;
    }

    m_st_n_ev_input_now ++;
    uint64_t next_p_ring_write = m_count_ring_write % m_size_ring;
    if(next_p_ring_write == m_hot_p_read){
      // buffer full, permanent data lose
      m_st_n_ev_overflow_now ++;
      continue;
    }

    uint16_t tg_l15 = 0x7fff & df->GetCounter();
    //std::cout<< "id "<< tg_l15 <<"  ";
    if(flag_wait_first_event){
      flag_wait_first_event = false;
      m_extension = df->GetExtension() ;
      tg_expected = tg_l15;
      m_st_n_tg_ev_begin = tg_expected-1;
    }
    if(tg_l15 != (tg_expected & 0x7fff)){
      // std::cout<<(tg_expected & 0x7fff)<< " " << tg_l15<<"\n";
      uint32_t tg_guess_0 = (tg_expected & 0xffff8000) + tg_l15;
      uint32_t tg_guess_1 = (tg_expected & 0xffff8000) + 0x8000 + tg_l15;
      if(tg_guess_0 > tg_expected && tg_guess_0 - tg_expected < 200){
        std::cout<< "missing trigger, expecting : provided "<< (tg_expected & 0x7fff) << " : "<< tg_l15<<" ("<< m_extension <<") \n";
        tg_expected =tg_guess_0;
      }
      else if (tg_guess_1 > tg_expected && tg_guess_1 - tg_expected < 200){
        std::cout<< "missing trigger, expecting : provided "<< (tg_expected & 0x7fff) << " : "<< tg_l15<<" ("<< m_extension <<") \n";
        tg_expected =tg_guess_1;
      }
      else{
        std::cout<< "broken trigger ID, expecting : provided "<< (tg_expected & 0x7fff) << " : "<< tg_l15<<" ("<<df->GetExtension() <<") \n";
        tg_expected ++;
        m_st_n_ev_bad_now ++;
        // permanent data lose
        continue;
      }
    }
    df->SetTrigger(tg_expected-1); //TODO: fix tlu firmware, mismatch between modes
    m_st_n_tg_ev_now = tg_expected-1;
    
    m_vec_ring_ev[next_p_ring_write] = df;
    m_count_ring_write ++;
    tg_expected ++;
  }
  m_rd->Close();
  std::fprintf(stdout, " rd stop  %s\n", m_rd->DeviceUrl().c_str());
  return m_count_ring_write;
}

uint64_t Layer::AsyncWatchDog(){
  m_tp_run_begin = std::chrono::system_clock::now();
  m_tp_old = m_tp_run_begin;
  m_is_async_watching = true;
  
  while(m_is_async_watching){
    std::this_thread::sleep_for(1s);
    uint64_t st_n_tg_ev_begin = m_st_n_tg_ev_begin;
    uint64_t st_n_tg_ev_now = m_st_n_tg_ev_now;
    uint64_t st_n_ev_input_now = m_st_n_ev_input_now;
    //uint64_t st_n_ev_output_now = m_st_n_ev_output_now;
    uint64_t st_n_ev_bad_now = m_st_n_ev_bad_now;
    uint64_t st_n_ev_overflow_now = m_st_n_ev_overflow_now;

    // time
    auto tp_now = std::chrono::system_clock::now();
    std::chrono::duration<double> dur_period_sec = tp_now - m_tp_old;
    std::chrono::duration<double> dur_accu_sec = tp_now - m_tp_run_begin;
    double sec_period = dur_period_sec.count();
    double sec_accu = dur_accu_sec.count();

    //std::cout<< "sec "<< sec_period<< " : " << sec_accu<<std::endl;

    // period
    uint64_t st_n_tg_ev_period = st_n_tg_ev_now - m_st_n_tg_ev_old;
    uint64_t st_n_ev_input_period = st_n_ev_input_now - m_st_n_ev_input_old;
    uint64_t st_n_ev_bad_period = st_n_ev_bad_now - m_st_n_ev_bad_old;
    uint64_t st_n_ev_overflow_period = st_n_ev_overflow_now - m_st_n_ev_overflow_old;
    
    // ratio
    //double st_output_vs_input_accu = st_n_ev_input_now? st_ev_output_now / st_ev_input_now : 1;
    double st_bad_vs_input_accu = st_n_ev_input_now? 1.0 * st_n_ev_bad_now / st_n_ev_input_now : 0;
    double st_overflow_vs_input_accu = st_n_ev_input_now? 1.0 *  st_n_ev_overflow_now / st_n_ev_input_now : 0;
    double st_input_vs_trigger_accu = st_n_ev_input_now? 1.0 * st_n_ev_input_now / (st_n_tg_ev_now - st_n_tg_ev_begin + 1) : 1;
    
    //double st_output_vs_input_period = st_ev_input_period? st_ev_output_period / st_ev_input_period : 1;
    double st_bad_vs_input_period = st_n_ev_input_period? 1.0 * st_n_ev_bad_period / st_n_ev_input_period : 0;
    double st_overflow_vs_input_period = st_n_ev_input_period? 1.0 *  st_n_ev_overflow_period / st_n_ev_input_period : 0;
    double st_input_vs_trigger_period = st_n_tg_ev_period? 1.0 *  st_n_ev_input_period / st_n_tg_ev_period : 1;
    
    // hz
    double st_hz_tg_accu = (st_n_tg_ev_now - st_n_tg_ev_begin + 1) / sec_accu ;
    double st_hz_input_accu = st_n_ev_input_now / sec_accu ; 

    double st_hz_tg_period = st_n_tg_ev_period / sec_period ;
    double st_hz_input_period = st_n_ev_input_period / sec_period ;

    std::string st_string_new =
      FirmwarePortal::FormatString("L<%u> event(%d)/trigger(%d - %d)=Ev/Tr(%.4f) dEv/dTr(%.4f) tr_accu(%.2f hz) ev_accu(%.2f hz) tr_period(%.2f hz) ev_period(%.2f hz)",
                                   m_extension, st_n_ev_input_now, st_n_tg_ev_now, st_n_tg_ev_begin, st_input_vs_trigger_accu, st_input_vs_trigger_period,
                                   st_hz_tg_accu, st_hz_input_accu, st_hz_tg_period, st_hz_input_period
                                   );
    
    {
      std::unique_lock lk(m_mtx_st);
      m_st_string = std::move(st_string_new);
    }
    
    //write to old
    m_st_n_tg_ev_old = st_n_tg_ev_now;
    m_st_n_ev_input_old = st_n_ev_input_now;
    m_st_n_ev_bad_old = st_n_ev_bad_now;
    m_st_n_ev_overflow_old = st_n_ev_overflow_now;
    m_tp_old = tp_now;
  }
  return 0;
}

std::string  Layer::GetStatusString(){
  std::unique_lock lk(m_mtx_st);
  return m_st_string;
}

JadeDataFrameSP& Layer::Front(){
  if(m_count_ring_write > m_count_ring_read) {
    uint64_t next_p_ring_read = m_count_ring_read % m_size_ring;
    m_hot_p_read = next_p_ring_read;
    // keep hot read to prevent write-overlapping
    return m_vec_ring_ev[next_p_ring_read];
  }
  else{
    return m_ring_end;
  }
}

void Layer::PopFront(){
  if(m_count_ring_write > m_count_ring_read) {
    uint64_t next_p_ring_read = m_count_ring_read % m_size_ring;
    m_hot_p_read = next_p_ring_read;
    // keep hot read to prevent write-overlapping
    m_vec_ring_ev[next_p_ring_read].reset();
    m_count_ring_read ++;
  }
}

uint64_t Layer::Size(){
  return  m_count_ring_write - m_count_ring_read;
}

void Layer::ClearBuffer(){
  m_count_ring_write = m_count_ring_read;
  m_vec_ring_ev.clear();
}

Telescope::Telescope(const std::string& file_context){
  rapidjson::GenericDocument<rapidjson::UTF8<char>, rapidjson::CrtAllocator>  js_doc;
  js_doc.Parse(file_context);

  if(js_doc.HasParseError()){
    fprintf(stderr, "JSON parse error: %s (at string positon %u) \n", rapidjson::GetParseError_En(js_doc.GetParseError()), js_doc.GetErrorOffset());
    throw;
  }
  const auto &js_obj = js_doc.GetObject();
  
  if(!js_obj.HasMember("telescope")){
    fprintf(stderr, "JSON configure file error: no \"telescope\" section \n");
    throw;
  }
  
  if(!js_obj.HasMember("testbeam")){
    fprintf(stderr, "JSON configure file error: no \"testbeam\" section \n");
    throw;
  }

  if(!js_obj.HasMember("layers")){
    fprintf(stderr, "JSON configure file error: no \"layers\" section \n");
    throw;
  }
  
  const auto& js_telescope  = js_obj["telescope"];
  const auto& js_testbeam   = js_obj["testbeam"];
  const auto& js_layers     = js_obj["layers"];

  m_js_telescope.CopyFrom<rapidjson::CrtAllocator>(js_telescope, m_jsa);
  m_js_testbeam.CopyFrom<rapidjson::CrtAllocator>(js_testbeam, m_jsa);
  
  std::map<std::string, double> layer_loc;
  std::multimap<double, std::string> loc_layer;
 
  if(!js_telescope.HasMember("locations")){
    fprintf(stderr, "JSON configure file error: no \"location\" section \n");
    throw;
  }

  // throw;
  for(const auto& l: js_telescope["locations"].GetObject()){
    std::string name = l.name.GetString();
    double loc = l.value.GetDouble();
    layer_loc[name] = loc;
    loc_layer.insert(std::pair<double, std::string>(loc, name));
  }
  
  for(const auto& l: loc_layer){
    std::string layer_name = l.second;
    bool layer_found = false;
    for (const auto& js_layer : js_layers.GetArray()){
      if(js_layer.HasMember("name") && js_layer["name"]==layer_name){
        std::unique_ptr<Layer> l(new Layer);
        l->m_fw.reset(new FirmwarePortal(FirmwarePortal::Stringify(js_layer["ctrl_link"])));
        l->m_rd.reset(new AltelReader(FirmwarePortal::Stringify(js_layer["data_link"])));
        l->m_name=layer_name;
        m_vec_layer.push_back(std::move(l));
        layer_found = true;
        break;
      }
    }
    if(!layer_found){
      std::fprintf(stderr, "Layer %6s: is not found in configure file \n", layer_name.c_str());
      throw;
    }
    std::fprintf(stdout, "Layer %6s:     at location Z = %8.2f\n", layer_name.c_str(), l.first);
  }

  if(!m_js_telescope.HasMember("config")){
      std::fprintf(stderr, "JSON configure file error: no telescope config \n");
      throw;
  }
  
  const auto& js_tele_conf = m_js_telescope["config"];
  for(auto &l: m_vec_layer){
    std::string name = l->m_name;
    if(!js_tele_conf.HasMember(name)){
      std::fprintf(stderr, "JSON configure file error: no config %s \n", name.c_str());
      throw;
    } 
    l->m_js_conf.CopyFrom(js_tele_conf[name], l->m_jsa);
  }
}

Telescope::~Telescope(){
  Stop();
}

std::vector<JadeDataFrameSP> Telescope::ReadEvent(){
  std::vector<JadeDataFrameSP> ev_sync;
  if (!m_is_running) return ev_sync;  
  
  uint32_t trigger_n = -1;
  for(auto &l: m_vec_layer){
    if( l->Size() == 0){
      // TODO check cached size of all layers
      return ev_sync;
    }
    else{
      uint32_t trigger_n_ev = l->Front()->GetTrigger();
      if(trigger_n_ev< trigger_n)
        trigger_n = trigger_n_ev;
    }
  }

  for(auto &l: m_vec_layer){
    auto &ev_front = l->Front(); 
    if(ev_front->GetTrigger() == trigger_n){
      ev_sync.push_back(ev_front);
      l->PopFront();
    }
  }
  
  if(ev_sync.size() < m_vec_layer.size() ){
    std::cout<< "dropped assambed event with subevent less than requried "<< m_vec_layer.size() <<" sub events" <<std::endl;
    std::string dev_numbers;
    for(auto & ev : ev_sync){
      dev_numbers += std::to_string(ev->GetExtension());
      dev_numbers +=" ";
    }
    std::cout<< "  tluID="<<trigger_n<<" subevent= "<< dev_numbers <<std::endl;
    std::vector<JadeDataFrameSP> empty;
    return empty;
  }
  if(m_mon_ev_read == m_mon_ev_write){
    m_ev_last=ev_sync;
    m_mon_ev_write ++;
  }  
  return ev_sync;
}

std::vector<JadeDataFrameSP> Telescope::ReadEvent_Lastcopy(){
  if(m_mon_ev_write > m_mon_ev_read){
    std::vector<JadeDataFrameSP> re_ev_last = m_ev_last;
    m_mon_ev_read ++;
    return re_ev_last;
  }
  else
    return m_ev_last_empty;
}

void Telescope::Init(){
  for(auto & l: m_vec_layer){
    l->fw_init();
  }
}

void Telescope::Start(){
  for(auto & l: m_vec_layer){
    l->rd_start();
  }
  
  for(auto & l: m_vec_layer){
    l->fw_start();
  }
  std::fprintf(stdout, "tel_start \n");

  if(!m_is_async_watching){
    m_fut_async_watch = std::async(std::launch::async, &Telescope::AsyncWatchDog, this);
  }
  
  m_fut_async_rd = std::async(std::launch::async, &Telescope::AsyncRead, this);
  m_is_running = true;
}

void Telescope::Start_no_tel_reading(){ // TO be removed, 
  for(auto & l: m_vec_layer){
    l->rd_start();
  }
  for(auto & l: m_vec_layer){
    l->fw_start();
  }
  if(!m_is_async_watching){
    m_fut_async_watch = std::async(std::launch::async, &Telescope::AsyncWatchDog, this);
  }
  //m_fut_async_rd = std::async(std::launch::async, &Telescope::AsyncRead, this);  
  m_is_running = true;
}

void Telescope::Stop(){
  m_is_async_reading = false;
  if(m_fut_async_rd.valid())
    m_fut_async_rd.get();

  m_is_async_watching = false;
  if(m_fut_async_watch.valid())
    m_fut_async_watch.get();
  
  for(auto & l: m_vec_layer){
    l->fw_stop();
  }
  
  for(auto & l: m_vec_layer){
    l->rd_stop();
  }

  for(auto & l: m_vec_layer){
    l->ClearBuffer();
  }
  
  m_is_running = false;
}

uint64_t Telescope::AsyncRead(){
  auto now = std::chrono::system_clock::now();
  auto now_c = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss<<std::put_time(std::localtime(&now_c), "%y%m%d%H%M%S");
  std::string now_str = ss.str();
  std::string data_path = "data/alpide_"+now_str+".json";
  FILE* fd = fopen(data_path.c_str(), "wb");
  rapidjson::StringBuffer js_sb;
  rapidjson::Writer<rapidjson::StringBuffer> js_writer;
  js_writer.SetMaxDecimalPlaces(5);
  uint64_t n_ev = 0;
  m_flag_next_event_add_conf = true;
  m_is_async_reading = true;
  while (m_is_async_reading){
    auto ev = ReadEvent();
    if(ev.empty()){
      std::this_thread::sleep_for(100us);
      continue;
    }

    n_ev ++;
    m_st_n_ev ++;
    //continue;
    js_sb.Clear();
    if(n_ev == 1){
      std::fwrite(reinterpret_cast<const char *>("[\n"), 1, 2, fd);
    }
    else{
      std::fwrite(reinterpret_cast<const char *>(",\n"), 1, 2, fd);
    }
    
    js_writer.Reset(js_sb);
    js_writer.StartObject();
    if(m_flag_next_event_add_conf){
      rapidjson::PutN(js_sb, '\n', 1);
      js_writer.String("testbeam");
      m_js_testbeam.Accept(js_writer);
      js_writer.String("telescope");
      m_js_telescope.Accept(js_writer);
      m_flag_next_event_add_conf = false;
    }
    if(m_count_st_js_write > m_count_st_js_read){
      rapidjson::PutN(js_sb, '\n', 1);
      js_writer.String("status");
      m_js_status.Accept(js_writer);
      m_count_st_js_read ++;
    }
    
    rapidjson::PutN(js_sb, '\n', 1);
    js_writer.String("layers");
    js_writer.StartArray();
    for(auto& e: ev){
      auto js_e = e->JSON(m_jsa);
      js_e.Accept(js_writer);
      rapidjson::PutN(js_sb, '\n', 1);
    }
    js_writer.EndArray();
    js_writer.EndObject();
    rapidjson::PutN(js_sb, '\n', 1);
    auto p_ch = js_sb.GetString();
    auto n_ch = js_sb.GetSize();
    std::fwrite(reinterpret_cast<const char *>(p_ch), 1, n_ch, fd);
  }
  
  std::fwrite(reinterpret_cast<const char *>("]"), 1, 2, fd);
  fclose(fd);
  std::fprintf(stdout, "Tele: disk file closed\n");
  return n_ev;
}

uint64_t Telescope::AsyncWatchDog(){
  m_is_async_watching = true;
  while(m_is_async_watching){
    rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator> js_status(rapidjson::kObjectType);
    std::this_thread::sleep_for(1s);
    for(auto &l: m_vec_layer){
      std::string l_status = l->GetStatusString();
      std::fprintf(stdout, "%s\n", l_status.c_str());
      
      rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator> name;
      rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator> value;
      name.SetString(l->m_name, m_jsa);
      value.SetString(l_status, m_jsa);
      js_status.AddMember(std::move(name), std::move(value), m_jsa);      
    }
    uint64_t st_n_ev = m_st_n_ev;
    std::fprintf(stdout, "Tele: disk saved events(%u) \n\n", st_n_ev);
    m_flag_next_event_add_conf = true;

    //TODO: make a json object to keep status;
    if(m_count_st_js_read == m_count_st_js_write){
      std::stringstream ss;
      std::time_t t = std::time(nullptr);
      ss<<std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
      std::string now_str = ss.str();
      js_status.AddMember("time", std::move(now_str), m_jsa);
      m_js_status = std::move(js_status);
      m_count_st_js_write ++;
      
    }
  }
  //sleep and watch running time status;
  return 0;
}
