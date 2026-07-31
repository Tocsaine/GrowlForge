#include "../src/state/PresetManager.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main(){
 using namespace growlforge;
 ParameterStore store(nullptr);
 PresetManager presets(store);
 if(presets.presetCount()<8){std::cerr<<"too few factory presets\n";return 1;}
 store.values[Drive]=6.7;store.values[Fuzz]=2.3;store.values[Bypass]=1.0;
 auto dir=std::filesystem::temp_directory_path()/"GrowlForgePresetTest";
 std::filesystem::create_directories(dir);
 auto file=dir/"Unit.gfpreset";
 if(!presets.saveFile(file,"Unit")){std::cerr<<"save failed\n";return 2;}
 std::ifstream stream(file);std::string text((std::istreambuf_iterator<char>(stream)),{});
 if(text.find("\"drive\": 6.7000")==std::string::npos){std::cerr<<"drive missing\n";return 3;}
 if(text.find("\"bypass\"")!=std::string::npos){std::cerr<<"bypass persisted in preset\n";return 4;}
 store.values[Drive]=0.0;store.values[Fuzz]=0.0;store.values[Bypass]=1.0;
 if(!presets.loadFile(file)){std::cerr<<"load failed\n";return 5;}
 if(std::abs(store.values[Drive].load()-6.7)>1e-6||std::abs(store.values[Fuzz].load()-2.3)>1e-6){std::cerr<<"values wrong\n";return 6;}
 if(store.values[Bypass].load()!=1.0){std::cerr<<"bypass changed by preset\n";return 7;}
 std::filesystem::remove_all(dir);
 std::cout<<"preset validation: ok\n";
}
