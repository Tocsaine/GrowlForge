#include <clap/clap.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstring>

namespace {
const void* ext(const clap_host_t*,const char*){return nullptr;} void noop(const clap_host_t*){}
}
int main(int argc,char**argv){if(argc!=2)return 2;void*lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib)return 3;auto*e=(const clap_plugin_entry_t*)dlsym(lib,"clap_entry");if(!e||!e->init(argv[1]))return 4;auto*f=(const clap_plugin_factory_t*)e->get_factory(CLAP_PLUGIN_FACTORY_ID);auto*d=f->get_plugin_descriptor(f,0);clap_host_t h{};h.clap_version=CLAP_VERSION;h.name="Inspect";h.vendor="GF";h.url="";h.version="1";h.get_extension=ext;h.request_restart=noop;h.request_process=noop;h.request_callback=noop;auto*p=f->create_plugin(f,&h,d->id);if(!p||!p->init(p))return 5;auto*pe=(const clap_plugin_params_t*)p->get_extension(p,CLAP_EXT_PARAMS);std::printf("id=%s\nname=%s\nparams=%u\n",d->id,d->name,pe->count(p));for(uint32_t i=0;i<pe->count(p);++i){clap_param_info_t x{};pe->get_info(p,i,&x);std::printf("%u|%u|%s|%s|%.17g|%.17g|%.17g\n",x.id,x.flags,x.name,x.module,x.min_value,x.max_value,x.default_value);}p->destroy(p);e->deinit();dlclose(lib);}
