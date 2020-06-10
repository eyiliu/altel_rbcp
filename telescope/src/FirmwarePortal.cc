
#include "FirmwarePortal.hh"
#include "rbcp.h"

FirmwarePortal::FirmwarePortal(const std::string &json_str, const std::string &ipaddr){
  m_alpide_ip_addr = ipaddr;
  m_json.Parse(json_str.c_str());
  if(m_json.HasParseError()){
    fprintf(stderr, "JSON parse error: %s (at string positon %u)", rapidjson::GetParseError_En(m_json.GetParseError()), m_json.GetErrorOffset());
    throw;
  }    
}


FirmwarePortal::FirmwarePortal(const std::string &json_str){
  rapidjson::Document js_doc;
  js_doc.Parse(json_str.c_str());
  if(js_doc.HasParseError()){
    fprintf(stderr, "JSON parse error: %s (at string positon %u)", rapidjson::GetParseError_En(js_doc.GetParseError()), js_doc.GetErrorOffset());
    throw;
  }

  auto& js_proto = js_doc["protocol"];
  auto& js_opt = js_doc["options"];
  std::string reg_file_path;
  if(js_proto == "udp"){
    m_alpide_ip_addr = js_opt["ip"].GetString();
    reg_file_path = js_opt["path"].GetString();
  }

  if(reg_file_path.empty()){
      fprintf(stderr, "empty reg_file_path");
      throw;
  }
  
  std::string reg_str = LoadFileToString(reg_file_path);
  if(reg_str.empty()){
    fprintf(stderr, "empty reg_str");
    throw;
  }
  
  m_json.Parse(reg_str.c_str());
  if(m_json.HasParseError()){
    fprintf(stderr, "JSON parse error: %s (at string positon %u)", rapidjson::GetParseError_En(m_json.GetParseError()), m_json.GetErrorOffset());
    throw;
  }

}



void  FirmwarePortal::WriteByte(uint64_t address, uint64_t value){
  DebugFormatPrint(std::cout, "WriteByte( address=%#016x ,  value=%#016x )\n", address, value);
  // rbcp_write("131.169.133.173", 4660, static_cast<uint32_t>(address), static_cast<uint8_t>(value));
  rbcp r(m_alpide_ip_addr);
  std::string recvStr(100, 0);
  r.DispatchCommand("wrb",  address, value, NULL);
};


uint64_t FirmwarePortal::ReadByte(uint64_t address){
  DebugFormatPrint(std::cout, "ReadByte( address=%#016x)\n", address);
  uint8_t reg_value;
  // rbcp_read("131.169.133.173", 4660, static_cast<uint32_t>(address), reg_value);
  rbcp r(m_alpide_ip_addr);
  std::string recvStr(100, 0);
  r.DispatchCommand("rd", address, 1, &recvStr); 
  reg_value=recvStr[0];
  DebugFormatPrint(std::cout, "ReadByte( address=%#016x) return value=%#016x\n", address, reg_value);
  return reg_value;
};

void FirmwarePortal::SetFirmwareRegister(const std::string& name, uint64_t value){
  DebugFormatPrint(std::cout, "INFO<%s>: %s( name=%s ,  value=%#016x )\n", __func__, __func__, name.c_str(), value);
  static const std::string array_name("FIRMWARE_REG_LIST_V2");
  auto& json_array = m_json[array_name];
  if(json_array.Empty()){
    FormatPrint(std::cerr, "ERROR<%s>:   unable to find array<%s>\n", __func__, array_name.c_str());
    throw;
  }
  bool flag_found_reg = false;
  for(auto& json_reg: json_array.GetArray()){
    if( json_reg["name"] != name )
      continue;
    auto& json_addr = json_reg["address"];
    if(json_addr.IsString()){
      uint64_t address = std::stoull(json_reg["address"].GetString(), 0, 16);
      WriteByte(address, value); //word = byte
    }
    else if(json_addr.IsArray()){
      auto& json_bytes = json_reg["bytes"];
      auto& json_words = json_reg["words"];
      if(!json_bytes.IsUint64()){
	FormatPrint(std::cerr, "ERROR<%s>:   bytes<%s> is not an int\n", __func__, Stringify(json_bytes).c_str());
	throw;
      }
      if(!json_words.IsUint64()){
	FormatPrint(std::cerr, "ERROR<%s>:   words<%s> is not an int\n", __func__, Stringify(json_words).c_str());
	throw;
      }
      uint64_t n_bytes = json_bytes.GetUint64();
      uint64_t n_words = json_words.GetUint64();
      if((!n_bytes)||(!n_words)||(n_bytes%n_words)){
	FormatPrint(std::cerr, "ERROR<%s>: incorrect bytes<%u> or words<%u>\n" , __func__, json_bytes.GetUint64(), json_words.GetUint64());
	throw;
      }
      uint64_t n_bits_per_word = 8*n_bytes/n_words;
      
      auto& json_endian = json_reg["endian"];
      bool flag_is_little_endian;
      if(json_endian=="LE"){
	flag_is_little_endian = true;
      }
      else if(json_endian=="BE"){
	flag_is_little_endian = false;
      }
      else{
	FormatPrint(std::cerr, "ERROR<%s>: unknown endian<%s>\n", __func__, Stringify(json_endian).c_str());
	throw;
      }

      if(n_words != json_addr.Size()){
	FormatPrint(std::cerr, "ERROR<%s>: address<%s> array's size does not match the word number which is %u\n", __func__ , Stringify(json_addr).c_str(), n_words);
	throw;
      }
      uint64_t i=0;
      for(auto& name_in_array: json_addr.GetArray()){
	if(!name_in_array.IsString()){
	  FormatPrint(std::cerr, "ERROR<%s>: name<%s> is not a string value\n", __func__, Stringify(name_in_array).c_str());
	  throw;
	}
	std::string name_in_array_str = name_in_array.GetString();
	uint64_t sub_value;
	if(flag_is_little_endian){
	  uint64_t f = 8*sizeof(value)-n_bits_per_word*(i+1);
	  uint64_t b = 8*sizeof(value)-n_bits_per_word;
	  sub_value = (value<<f)>>b;
	  DebugFormatPrint(std::cout, "INFO<%s>: %s value=%#016x (<<%u)  (>>%u) sub_value=%#016x \n", __func__, "LE", value, f, b, sub_value);
	}
	else{
	  uint64_t f = 8*sizeof(value)-n_bits_per_word*(n_words-i);
	  uint64_t b = 8*sizeof(value)-n_bits_per_word;
	  sub_value = (value<<f)>>b;
	  DebugFormatPrint(std::cout, "INFO<%s>: %s value=%#016x (<<%u)  (>>%u) sub_value=%#016x \n", __func__, "BE", value, f, b, sub_value);
	}
	SetFirmwareRegister(name_in_array_str, sub_value);
	i++;
      }
    }
    else{
      FormatPrint(std::cerr, "ERROR<%s>: unknown address format<%s>\n", __func__, Stringify(json_addr).c_str());
      throw;
    }
    flag_found_reg = true;
    break;
  }
  if(!flag_found_reg){
    FormatPrint(std::cerr, "ERROR<%s>: unable to find register<%s> in array<%s>\n", __func__, name.c_str(), array_name.c_str());
    throw;
  }
}

void FirmwarePortal::SetAlpideRegister(const std::string& name, uint64_t value){
  DebugFormatPrint(std::cout, "INFO<%s>: %s( name=%s ,  value=%#016x )\n", __func__, __func__, name.c_str(), value);
  static const std::string array_name("CHIP_REG_LIST");
  auto& json_array = m_json[array_name];
  if(json_array.Empty()){
    FormatPrint(std::cerr, "ERROR<%s>:   unable to find array<%s>\n", __func__, array_name.c_str());
    throw;
  }   
  
  bool flag_found_reg = false;
  for(auto& json_reg: json_array.GetArray()){
    if( json_reg["name"] != name )
      continue;
    auto& json_addr = json_reg["address"];
    if(json_addr.IsString()){
      uint64_t address = std::stoull(json_reg["address"].GetString(), 0, 16);
      SetFirmwareRegister("ADDR_CHIP_REG", address);
      SetFirmwareRegister("DATA_WRITE", value);
      SendFirmwareCommand("WRITE");
    }
    else if(json_addr.IsArray()){
      auto& json_bytes = json_reg["bytes"];
      auto& json_words = json_reg["words"];
      if(!json_bytes.IsUint64()){
	FormatPrint(std::cerr, "ERROR<%s>:   bytes<%s> is not an int\n", __func__, Stringify(json_bytes).c_str());
	throw;
      }
      if(!json_words.IsUint64()){
	FormatPrint(std::cerr, "ERROR<%s>:   words<%s> is not an int\n", __func__, Stringify(json_words).c_str());
	throw;
      }
      uint64_t n_bytes = json_bytes.GetUint64();
      uint64_t n_words = json_words.GetUint64();
      if((!n_bytes)||(!n_words)||(n_bytes%n_words)){
	FormatPrint(std::cerr, "ERROR<%s>: incorrect bytes<%u> or words<%u>\n" , __func__, json_bytes.GetUint64(), json_words.GetUint64());
	throw;
      }
      uint64_t n_bits_per_word = 8*n_bytes/n_words;

      auto& json_endian = json_reg["endian"];
      bool flag_is_little_endian;
      if(json_endian=="LE"){
	flag_is_little_endian = true;
      }
      else if(json_endian=="BE"){
	flag_is_little_endian = false;
      }
      else{
	FormatPrint(std::cerr, "ERROR<%s>: unknown endian<%s>\n", __func__, Stringify(json_endian).c_str());
	throw;
      }
      
      if(n_words != json_addr.Size()){
	FormatPrint(std::cerr, "ERROR<%s>: address<%s> array's size does not match the word number which is %u\n", __func__ ,Stringify(json_addr).c_str(), n_words);
	throw;
      }
      uint64_t i=0;
      for(auto& name_in_array: json_addr.GetArray() ){
	if(!name_in_array.IsString()){
	  FormatPrint(std::cerr, "ERROR<%s>: name<%s> is not a string value\n", __func__, Stringify(name_in_array).c_str());
	  throw;
	}
	std::string name_in_array_str = name_in_array.GetString();
	uint64_t sub_value;
	if(flag_is_little_endian){
	  uint64_t f = (8*sizeof(value)-n_bits_per_word*(i+1));
	  uint64_t b = (8*sizeof(value)-n_bits_per_word);
	  sub_value = (value<<f)>>b;
	  DebugFormatPrint(std::cout, "INFO<%s>:  %s value=%#016x << %u  >>%u sub_value=%#016x \n", __func__, "LE", value, f, b, sub_value);
	}
	else{
	  uint64_t f = (8*sizeof(value)-n_bits_per_word*(n_words-i));
	  uint64_t b = (8*sizeof(value)-n_bits_per_word);
	  sub_value = (value<<f)>>b;
	  DebugFormatPrint(std::cout, "INFO<%s>:  %s value=%#016x << %u  >>%u sub_value=%#016x \n", __func__, "BE", value, f, b, sub_value);
	}
	SetAlpideRegister(name_in_array_str, sub_value);
	i++;  
      }
    }
    else{
      FormatPrint(std::cerr, "ERROR<%s>: unknown address format<%s>\n", __func__, Stringify(json_addr).c_str());
      throw;
    }
    flag_found_reg = true;
    break;
  }
  if(!flag_found_reg){
    FormatPrint(std::cerr, "ERROR<%s>: unable to find register<%s> in array<%s>\n", __func__, name.c_str(), array_name.c_str());
    throw;
  }
}

void FirmwarePortal::SendFirmwareCommand(const std::string& name){
  DebugFormatPrint(std::cout, "INFO<%s>:  %s( name=%s )\n", __func__, __func__, name.c_str());
  static const std::string array_name("FIRMWARE_CMD_LIST_V2");
  auto& json_array = m_json[array_name];
  if(json_array.Empty()){
    FormatPrint(std::cerr, "ERROR<%s>:   unable to find array<%s>\n", __func__, array_name.c_str());
    throw;
  }
  bool flag_found_cmd = false;
  for(auto& json_cmd: json_array.GetArray()){
    if( json_cmd["name"] != name )
      continue;
    auto& json_value = json_cmd["value"];
    if(!json_value.IsString()){
      FormatPrint(std::cerr, "ERROR<%s>:   command value<%s> is not a string\n", __func__, Stringify(json_value).c_str());
      throw;
    }
    uint64_t cmd_value = std::stoull(json_value.GetString(),0,16);
    SetFirmwareRegister("FIRMWARE_CMD", cmd_value);
    flag_found_cmd = true;
  }
  if(!flag_found_cmd){
    FormatPrint(std::cerr, "ERROR<%s>: unable to find command<%s> in array<%s>\n", __func__, name.c_str(), array_name.c_str());
    throw;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void FirmwarePortal::SendAlpideCommand(const std::string& name){
  DebugFormatPrint(std::cout, "INFO<%s>:  %s( name=%s )\n", __func__, __func__, name.c_str());
  static const std::string array_name("CHIP_CMD_LIST");
  auto& json_array = m_json[array_name];
  if(json_array.Empty()){
    FormatPrint(std::cerr, "ERROR<%s>:   unable to find array<%s>\n", __func__, array_name.c_str());
    throw;
  }
  bool flag_found_cmd = false;
  for(auto& json_cmd: json_array.GetArray()){
    if( json_cmd["name"] != name )
      continue;
    auto& json_value = json_cmd["value"];
    if(!json_value.IsString()){
      FormatPrint(std::cerr, "ERROR<%s>:   command value<%s> is not a string\n", __func__, Stringify(json_value).c_str());
      throw;
    }
    uint64_t cmd_value = std::stoull(json_value.GetString(),0,16);
    SetAlpideRegister("CHIP_CMD", cmd_value);
    flag_found_cmd = true;
  }
  if(!flag_found_cmd){
    FormatPrint(std::cerr, "ERROR<%s>: unable to find command<%s> in array<%s>\n", __func__, name.c_str(), array_name.c_str());
    throw;
  } 
}

void FirmwarePortal::SendAlpideBroadcast(const std::string& name){
  DebugFormatPrint(std::cout, "INFO<%s>:  %s( name=%s )\n", __func__, __func__, name.c_str());
  static const std::string array_name("CHIP_CMD_LIST");
  auto& json_array = m_json[array_name];
  if(json_array.Empty()){
    FormatPrint(std::cerr, "ERROR<%s>:   unable to find array<%s>\n", __func__, array_name.c_str());
    throw;
  }
  bool flag_found_cmd = false;
  for(auto& json_cmd: json_array.GetArray()){
    if( json_cmd["name"] != name )
      continue;
    auto& json_value = json_cmd["value"];
    if(!json_value.IsString()){
      FormatPrint(std::cerr, "ERROR<%s>:   command value<%s> is not a string\n", __func__, Stringify(json_value).c_str());
      throw;
    }
    uint64_t cmd_value = std::stoull(json_value.GetString(),0,16);
    SetFirmwareRegister("BROADCAST_OPCODE", cmd_value);
    SendFirmwareCommand("BROADCAST");
    flag_found_cmd = true;
  }
  if(!flag_found_cmd){
    FormatPrint(std::cerr, "ERROR<%s>: unable to find command<%s> in array<%s>\n", __func__, name.c_str(), array_name.c_str());
    throw;
  }
}


uint64_t FirmwarePortal::GetFirmwareRegister(const std::string& name){
  DebugFormatPrint(std::cout, "INFO<%s>:  %s( name=%s )\n", __func__, __func__, name.c_str());
  static const std::string array_name("FIRMWARE_REG_LIST_V2");
  auto& json_array = m_json[array_name];
  if(json_array.Empty()){
    FormatPrint(std::cerr, "ERROR<%s>:   unable to find array<%s>\n", __func__, array_name.c_str());
    throw;
  }
  uint64_t value;
  bool flag_found_reg = false;
  for(auto& json_reg: json_array.GetArray()){
    if( json_reg["name"] != name )
      continue;
    auto& json_addr = json_reg["address"];
    if(json_addr.IsString()){
      uint64_t address = std::stoull(json_reg["address"].GetString(), 0, 16);
      value = ReadByte(address);
    }
    else if(json_addr.IsArray()){
      auto& json_bytes = json_reg["bytes"];
      auto& json_words = json_reg["words"];
      if(!json_bytes.IsUint64()){
	FormatPrint(std::cerr, "ERROR<%s>:   bytes<%s> is not an int\n", __func__, Stringify(json_bytes).c_str());
	throw;
      }
      if(!json_words.IsUint64()){
	FormatPrint(std::cerr, "ERROR<%s>:   words<%s> is not an int\n", __func__, Stringify(json_words).c_str());
	throw;
      }
      uint64_t n_bytes = json_bytes.GetUint64();
      uint64_t n_words = json_words.GetUint64();
      if((!n_bytes)||(!n_words)||(n_bytes%n_words)){
	FormatPrint(std::cerr, "ERROR<%s>: incorrect bytes<%u> or words<%u>\n" , __func__, json_bytes.GetUint64(), json_words.GetUint64());
	throw;
      }
      uint64_t n_bits_per_word = 8*n_bytes/n_words;

      auto& json_endian = json_reg["endian"];
      bool flag_is_little_endian;
      if(json_endian=="LE"){
	flag_is_little_endian = true;
      }
      else if(json_endian=="BE"){
	flag_is_little_endian = false;
      }
      else{
	FormatPrint(std::cerr, "ERROR<%s>: unknown endian<%s>\n", __func__, Stringify(json_endian).c_str());
	throw;
      }

      if(n_words != json_addr.Size()){
	FormatPrint(std::cerr, "ERROR<%s>: address<%s> array's size does not match the word number which is %u\n", __func__ , Stringify(json_addr).c_str(), n_words);
	throw;
      }
      uint64_t i=0;
      value = 0;
      for(auto& name_in_array: json_addr.GetArray() ){
	if(!name_in_array.IsString()){
	  FormatPrint(std::cerr, "ERROR<%s>: name<%s> is not a string value\n", __func__, Stringify(name_in_array).c_str());
	  throw;
	}
	std::string name_in_array_str = name_in_array.GetString();
	uint64_t sub_value = GetFirmwareRegister(name_in_array_str);
	uint64_t add_value;
	if(flag_is_little_endian){
	  uint64_t f = n_bits_per_word*i;
	  uint64_t b = 0;
	  add_value = (sub_value<<f)>>b;
	  DebugFormatPrint(std::cout, "INFO<%s>:  %s sub_value=%#016x << %u  >>%u add_value=%#016x \n", __func__, "LE", sub_value, f, b, add_value);
	}
	else{
	  uint64_t f = n_bits_per_word*(n_words-1-i);
	  uint64_t b = 0;
	  add_value = (sub_value<<f)>>b;
	  DebugFormatPrint(std::cout, "INFO<%s>:  %s sub_value=%#016x << %u  >>%u add_value=%#016x \n", __func__, "BE", sub_value, f, b, add_value);
	}
	value += add_value;
	i++;
      }
    }
    else{
      FormatPrint(std::cerr, "ERROR<%s>: unknown address format<%s>\n", __func__, Stringify(json_addr).c_str());
      throw;
    }
    flag_found_reg = true;
    break;
  }
  if(!flag_found_reg){
    FormatPrint(std::cerr, "ERROR<%s>: unable to find register<%s> in array<%s>\n", __func__, name.c_str(), array_name.c_str());
    throw;
  }
  DebugFormatPrint(std::cout, "INFO<%s>: %s( name=%s ) return value=%#016x \n", __func__, __func__, name.c_str(), value);
  return value;
}


uint64_t FirmwarePortal::GetAlpideRegister(const std::string& name){  
  DebugFormatPrint(std::cout, "INFO<%s>:  %s( name=%s )\n",__func__, __func__, name.c_str());
  static const std::string array_name("CHIP_REG_LIST");
  auto& json_array = m_json[array_name];
  if(json_array.Empty()){
    FormatPrint(std::cerr, "ERROR<%s>:   unable to find array<%s>\n", __func__, array_name.c_str());
    throw;
  }
  uint64_t value;
  bool flag_found_reg = false;
  for(auto& json_reg: json_array.GetArray()){
    if( json_reg["name"] != name )
      continue;
    auto& json_addr = json_reg["address"];
    if(json_addr.IsString()){
      uint64_t address = std::stoull(json_reg["address"].GetString(), 0, 16);
      SetFirmwareRegister("ADDR_CHIP_REG", address);
      uint64_t nr_old = GetFirmwareRegister("COUNT_READ");
      SendFirmwareCommand("READ");
      std::chrono::system_clock::time_point  tp_timeout = std::chrono::system_clock::now() +  std::chrono::milliseconds(100);
      bool flag_enable_counter_check = true; //TODO: enable it for a real hardware;
      if(!flag_enable_counter_check){
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	FormatPrint(std::cout, "WARN<%s>: checking of the read count is disabled\n", __func__);
      }
      while(flag_enable_counter_check){
	uint64_t nr_new = GetFirmwareRegister("COUNT_READ");
	if(nr_new != nr_old){
	  break;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	if(std::chrono::system_clock::now() > tp_timeout){
	  FormatPrint(std::cerr, "ERROR<%s>:  timeout to read back Alpide register<%s>\n", __func__, name.c_str());
	  throw;
	}
      }
      value = GetFirmwareRegister("DATA_READ");
    }
    else if(json_addr.IsArray()){
      auto& json_bytes = json_reg["bytes"];
      auto& json_words = json_reg["words"];
      if(!json_bytes.IsUint64()){
	FormatPrint(std::cerr, "ERROR<%s>:   bytes<%s> is not an int\n", __func__, Stringify(json_bytes).c_str());
	throw;
      }
      if(!json_words.IsUint64()){
	FormatPrint(std::cerr, "ERROR<%s>:   words<%s> is not an int\n", __func__, Stringify(json_words).c_str());
	throw;
      }
      uint64_t n_bytes = json_bytes.GetUint64();
      uint64_t n_words = json_words.GetUint64();
      if((!n_bytes)||(!n_words)||(n_bytes%n_words)){
	FormatPrint(std::cerr, "ERROR<%s>: incorrect bytes<%u> or words<%u>\n" , __func__, json_bytes.GetUint64(), json_words.GetUint64());
	throw;
      }
      uint64_t n_bits_per_word = 8*n_bytes/n_words;

      auto& json_endian = json_reg["endian"];
      bool flag_is_little_endian;
      if(json_endian=="LE"){
	flag_is_little_endian = true;
      }
      else if(json_endian=="BE"){
	flag_is_little_endian = false;
      }
      else{
	FormatPrint(std::cerr, "ERROR<%s>: unknown endian<%s>\n", __func__, Stringify(json_endian).c_str());
	throw;
      }

      if(n_words != json_addr.Size()){
	FormatPrint(std::cerr, "ERROR<%s>: address<%s> array's size does not match the word number which is %u\n", __func__ , Stringify(json_addr).c_str(), n_words);
	throw;
      }
      uint64_t i=0;
      value = 0;
      for(auto& name_in_array: json_addr.GetArray() ){
	if(!name_in_array.IsString()){
	  FormatPrint(std::cerr, "ERROR<%s>: name<%s> is not a string value\n", __func__, Stringify(name_in_array).c_str());
	  throw;
	}
	std::string name_in_array_str = name_in_array.GetString();
	uint64_t sub_value = GetAlpideRegister(name_in_array_str);
	uint64_t add_value;
	if(flag_is_little_endian){
	  uint64_t f = n_bits_per_word*i;
	  uint64_t b = 0;
	  add_value = (sub_value<<f)>>b;
	  DebugFormatPrint(std::cout, "INFO<%s>:  %s sub_value=%#016x << %u  >>%u add_value=%#016x \n", __func__, "LE", sub_value, f, b, add_value);
	}
	else{
	  uint64_t f = n_bits_per_word*(n_words-1-i);
	  uint64_t b = 0;
	  add_value = (sub_value<<f)>>b;
	  DebugFormatPrint(std::cout, "INFO<%s>:  %s sub_value=%#016x << %u  >>%u add_value=%#016x \n", __func__, "BE", sub_value, f, b, add_value);
	}
	value += add_value;
	i++;
      }
    }
    else{
      FormatPrint(std::cerr, "ERROR<%s>: unknown address format<%s>\n", __func__, Stringify(json_addr).c_str());
      throw;
    }
    flag_found_reg = true;
    break;
  }
  if(!flag_found_reg){
    FormatPrint(std::cerr, "ERROR<%s>: unable to find register<%s> in array<%s>\n", __func__, name.c_str(), array_name.c_str());
    throw;
  }  
  DebugFormatPrint(std::cout, "INFO<%s>: %s( name=%s ) return value=%#016x \n", __func__, __func__, name.c_str(), value);
  return value;  
}


std::string FirmwarePortal::LoadFileToString(const std::string& path){
  std::ifstream ifs(path);
  if(!ifs.good()){
    std::cerr<<"LoadFileToString:: ERROR, unable to load file<"<<path<<">\n";
    throw;
  }
  std::string str;
  str.assign((std::istreambuf_iterator<char>(ifs) ),
             (std::istreambuf_iterator<char>()));
  return str;
}


void FirmwarePortal::SetRegionRegister(uint64_t region, const std::string& name, uint64_t value){
  FormatPrint(std::cout, "INFO<%s>: %s( name=%s ,  value=%#016x )\n", __func__, __func__, name.c_str(), value);
  static const std::string array_name("CHIP_REG_LIST");
  auto& json_array = m_json[array_name];
  if(json_array.Empty()){
    FormatPrint(std::cerr, "ERROR<%s>:   unable to find array<%s>\n", __func__, array_name.c_str());
    throw;
  }   
  
  bool flag_found_reg = false;
  for(auto& json_reg: json_array.GetArray()){
    if( json_reg["name"] != name )
      continue;
    auto& json_addr = json_reg["address"];
    if(json_addr.IsString()){
      uint64_t address_region_local = (std::stoull(json_reg["address"].GetString(), 0, 16)) & 0x07ff;
      uint64_t address = (region << 11) + address_region_local;
      SetFirmwareRegister("ADDR_CHIP_REG", address);
      SetFirmwareRegister("DATA_WRITE", value);
      SendFirmwareCommand("WRITE");
    }
    else if(json_addr.IsArray()){
      auto& json_bytes = json_reg["bytes"];
      auto& json_words = json_reg["words"];
      if(!json_bytes.IsUint64()){
	FormatPrint(std::cerr, "ERROR<%s>:   bytes<%s> is not an int\n", __func__, Stringify(json_bytes).c_str());
	throw;
      }
      if(!json_words.IsUint64()){
	FormatPrint(std::cerr, "ERROR<%s>:   words<%s> is not an int\n", __func__, Stringify(json_words).c_str());
	throw;
      }
      uint64_t n_bytes = json_bytes.GetUint64();
      uint64_t n_words = json_words.GetUint64();
      if((!n_bytes)||(!n_words)||(n_bytes%n_words)){
	FormatPrint(std::cerr, "ERROR<%s>: incorrect bytes<%u> or words<%u>\n" , __func__, json_bytes.GetUint64(), json_words.GetUint64());
	throw;
      }
      uint64_t n_bits_per_word = 8*n_bytes/n_words;

      auto& json_endian = json_reg["endian"];
      bool flag_is_little_endian;
      if(json_endian=="LE"){
	flag_is_little_endian = true;
      }
      else if(json_endian=="BE"){
	flag_is_little_endian = false;
      }
      else{
	FormatPrint(std::cerr, "ERROR<%s>: unknown endian<%s>\n", __func__, Stringify(json_endian).c_str());
	throw;
      }
      
      if(n_words != json_addr.Size()){
	FormatPrint(std::cerr, "ERROR<%s>: address<%s> array's size does not match the word number which is %u\n", __func__ ,Stringify(json_addr).c_str(), n_words);
	throw;
      }
      uint64_t i=0;
      for(auto& name_in_array: json_addr.GetArray() ){
	if(!name_in_array.IsString()){
	  FormatPrint(std::cerr, "ERROR<%s>: name<%s> is not a string value\n", __func__, Stringify(name_in_array).c_str());
	  throw;
	}
	std::string name_in_array_str = name_in_array.GetString();
	uint64_t sub_value;
	if(flag_is_little_endian){
	  uint64_t f = (8*sizeof(value)-n_bits_per_word*(i+1));
	  uint64_t b = (8*sizeof(value)-n_bits_per_word);
	  sub_value = (value<<f)>>b;
	  DebugFormatPrint(std::cout, "INFO<%s>:  %s value=%#016x << %u  >>%u sub_value=%#016x \n", __func__, "LE", value, f, b, sub_value);
	}
	else{
	  uint64_t f = (8*sizeof(value)-n_bits_per_word*(n_words-i));
	  uint64_t b = (8*sizeof(value)-n_bits_per_word);
	  sub_value = (value<<f)>>b;
	  DebugFormatPrint(std::cout, "INFO<%s>:  %s value=%#016x << %u  >>%u sub_value=%#016x \n", __func__, "BE", value, f, b, sub_value);
	}
	SetRegionRegister(region, name_in_array_str, sub_value);
	i++;  
      }
    }
    else{
      FormatPrint(std::cerr, "ERROR<%s>: unknown address format<%s>\n", __func__, Stringify(json_addr).c_str());
      throw;
    }
    flag_found_reg = true;
    break;
  }
  if(!flag_found_reg){
    FormatPrint(std::cerr, "ERROR<%s>: unable to find register<%s> in array<%s>\n", __func__, name.c_str(), array_name.c_str());
    throw;
  }
  
}


void FirmwarePortal::BroadcastRegionRegister(const std::string& name, uint64_t value){
  DebugFormatPrint(std::cout, "INFO<%s>: %s( name=%s ,  value=%#016x )\n", __func__, __func__, name.c_str(), value);
  static const std::string array_name("CHIP_REG_LIST");
  auto& json_array = m_json[array_name];
  if(json_array.Empty()){
    FormatPrint(std::cerr, "ERROR<%s>:   unable to find array<%s>\n", __func__, array_name.c_str());
    throw;
  }   
  
  bool flag_found_reg = false;
  for(auto& json_reg: json_array.GetArray()){
    if( json_reg["name"] != name )
      continue;
    auto& json_addr = json_reg["address"];
    if(json_addr.IsString()){
      uint64_t address_region_local = (std::stoull(json_reg["address"].GetString(), 0, 16)) & 0x07ff;
      uint64_t address = address_region_local | 0x80; // 0b1000 0000 broadcast_bit
      SetFirmwareRegister("ADDR_CHIP_REG", address);
      SetFirmwareRegister("DATA_WRITE", value);
      SendFirmwareCommand("WRITE");
    }
    else if(json_addr.IsArray()){
      auto& json_bytes = json_reg["bytes"];
      auto& json_words = json_reg["words"];
      if(!json_bytes.IsUint64()){
	FormatPrint(std::cerr, "ERROR<%s>:   bytes<%s> is not an int\n", __func__, Stringify(json_bytes).c_str());
	throw;
      }
      if(!json_words.IsUint64()){
	FormatPrint(std::cerr, "ERROR<%s>:   words<%s> is not an int\n", __func__, Stringify(json_words).c_str());
	throw;
      }
      uint64_t n_bytes = json_bytes.GetUint64();
      uint64_t n_words = json_words.GetUint64();
      if((!n_bytes)||(!n_words)||(n_bytes%n_words)){
	FormatPrint(std::cerr, "ERROR<%s>: incorrect bytes<%u> or words<%u>\n" , __func__, json_bytes.GetUint64(), json_words.GetUint64());
	throw;
      }
      uint64_t n_bits_per_word = 8*n_bytes/n_words;

      auto& json_endian = json_reg["endian"];
      bool flag_is_little_endian;
      if(json_endian=="LE"){
	flag_is_little_endian = true;
      }
      else if(json_endian=="BE"){
	flag_is_little_endian = false;
      }
      else{
	FormatPrint(std::cerr, "ERROR<%s>: unknown endian<%s>\n", __func__, Stringify(json_endian).c_str());
	throw;
      }
      
      if(n_words != json_addr.Size()){
	FormatPrint(std::cerr, "ERROR<%s>: address<%s> array's size does not match the word number which is %u\n", __func__ ,Stringify(json_addr).c_str(), n_words);
	throw;
      }
      uint64_t i=0;
      for(auto& name_in_array: json_addr.GetArray() ){
	if(!name_in_array.IsString()){
	  FormatPrint(std::cerr, "ERROR<%s>: name<%s> is not a string value\n", __func__, Stringify(name_in_array).c_str());
	  throw;
	}
	std::string name_in_array_str = name_in_array.GetString();
	uint64_t sub_value;
	if(flag_is_little_endian){
	  uint64_t f = (8*sizeof(value)-n_bits_per_word*(i+1));
	  uint64_t b = (8*sizeof(value)-n_bits_per_word);
	  sub_value = (value<<f)>>b;
	  DebugFormatPrint(std::cout, "INFO<%s>:  %s value=%#016x << %u  >>%u sub_value=%#016x \n", __func__, "LE", value, f, b, sub_value);
	}
	else{
	  uint64_t f = (8*sizeof(value)-n_bits_per_word*(n_words-i));
	  uint64_t b = (8*sizeof(value)-n_bits_per_word);
	  sub_value = (value<<f)>>b;
	  DebugFormatPrint(std::cout, "INFO<%s>:  %s value=%#016x << %u  >>%u sub_value=%#016x \n", __func__, "BE", value, f, b, sub_value);
	}
	BroadcastRegionRegister(name_in_array_str, sub_value);
	i++;  
      }
    }
    else{
      FormatPrint(std::cerr, "ERROR<%s>: unknown address format<%s>\n", __func__, Stringify(json_addr).c_str());
      throw;
    }
    flag_found_reg = true;
    break;
  }
  if(!flag_found_reg){
    FormatPrint(std::cerr, "ERROR<%s>: unable to find register<%s> in array<%s>\n", __func__, name.c_str(), array_name.c_str());
    throw;
  }
}


void FirmwarePortal::SetPixelRegister(uint64_t x, uint64_t y, const std::string& name, uint64_t value){
  if(name!="MASK_EN" && name!="PULSE_EN"){
    FormatPrint(std::cerr, "ERROR<%s>: unable to find register<%s>. There are MASK_EN and PULSE_EN in pixel.\n", __func__, name.c_str());
    throw;
  }
  
  uint64_t bit_ROWREGM_DATA = value? 0x0002:0x0000;
  uint64_t bit_ROWREGM_SEL = (name=="MASK_EN")?0x0001:0x0000; // mismatch between pALPIDE-3 and ALPIDE manual, using p3
  uint64_t conf_value = bit_ROWREGM_DATA | bit_ROWREGM_SEL;

  uint64_t column_line_region_number = x/32;
  uint64_t column_line_number_local = x%32;
  uint64_t column_line_number_local_pattern = 1ULL << column_line_number_local;

  uint64_t row_line_region_number = y/16;
  uint64_t row_line_number_local = y%16;
  uint64_t row_line_number_local_pattern = 1ULL << row_line_number_local;
  
  BroadcastRegionRegister("REGION_COLUMN_SELECT", 0);
  BroadcastRegionRegister("REGION_ROW_SELECT", 0); //disable all selection lines
  BroadcastRegionRegister("REGION_TOGGLE_COLUMN_ROW", 0); //toggle all change
  
  SetAlpideRegister("PIX_CONF_GLOBAL", conf_value);
  SetRegionRegister(column_line_region_number, "REGION_COLUMN_SELECT", column_line_number_local_pattern);
  SetRegionRegister(row_line_region_number, "REGION_ROW_SELECT", row_line_number_local_pattern);
  BroadcastRegionRegister("REGION_TOGGLE_COLUMN_ROW", 0); //toggle all regional change (only selected)
}

void FirmwarePortal::BroadcastPixelRegister(const std::string& name, uint64_t value){
  if(name!="MASK_EN" && name!="PULSE_EN"){
    FormatPrint(std::cerr, "ERROR<%s>: unable to find register<%s>. There are MASK_EN and PULSE_EN in pixel.\n", __func__, name.c_str());
    throw;
  }
  
  uint64_t bit_ROWREGM_DATA = value? 0x0002:0x0000;
  uint64_t bit_ROWREGM_SEL = (name=="MASK_EN")?0x0001:0x0000;
  uint64_t conf_value = bit_ROWREGM_DATA | bit_ROWREGM_SEL;

  BroadcastRegionRegister("REGION_COLUMN_SELECT", 0);
  BroadcastRegionRegister("REGION_ROW_SELECT", 0); //disable all selection lines
  BroadcastRegionRegister("REGION_TOGGLE_COLUMN_ROW", 0); //toggle all change
  
  SetAlpideRegister("PIX_CONF_GLOBAL", conf_value);  
  BroadcastRegionRegister("REGION_COLUMN_SELECT", 0xffffffff);
  BroadcastRegionRegister("REGION_ROW_SELECT", 0xffff); //enable all selection lines
  BroadcastRegionRegister("REGION_TOGGLE_COLUMN_ROW", 0); //toggle all change
}

void FirmwarePortal::InjectPulse(){
  //SetAlpideRegister("FROMU_CONF_1", ); // APULSE/DPULSE
  SetAlpideRegister("FROMU_PULSING_2", 0xff); // duration  
  SendAlpideBroadcast("PULSE");
}


