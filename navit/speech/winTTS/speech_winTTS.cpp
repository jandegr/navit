#include <windows.h>
#include <sapi.h>

extern "C" {
#include "item.h"
#include "debug.h"
#include "plugin.h"
#include "speech.h"
}

struct speech_priv {
	ISpVoice * pVoice;
	int flags;
};


static void 
speech_winTTS_destroy(struct speech_priv *spch) {
	delete spch;
	CoUninitialize();
}

static int 
speech_winTTS_say(struct speech_priv *spch, const char *text) {
    const size_t cSize = strlen(text)+1;
    wchar_t* wc = new wchar_t[cSize];
    size_t tmp = 0;
    mbstowcs_s(&tmp, wc, cSize, text, cSize);
    spch->pVoice->Speak(wc, SVSFlagsAsync, NULL);
    delete wc;
    return 1;
}
	
static int
speech_winTTS_init(struct speech_priv *spch) {
	if (FAILED(::CoInitialize(NULL))){
		dbg(lvl_error,"Window Com init failed\n");
		return 0;
	}
    HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void **)&(spch->pVoice));    
	if(!SUCCEEDED(hr)){	
		dbg(lvl_error,"voice creation failed\n");	
	}
	return 1;
}
	
	
static struct speech_methods speech_winTTS_meth = {
	speech_winTTS_destroy,
	speech_winTTS_say,
};
	
	
static struct speech_priv *
speech_winTTS_new(struct speech_methods *meth, struct attr **attrs, struct attr *parent) {
	speech_priv *ret = new speech_priv;
	struct attr *flags;
	*meth=speech_winTTS_meth;

	if (!speech_winTTS_init(ret)) {
		dbg(lvl_error,"Failed to init winTTS speech \n");
		delete ret;
		ret = NULL;
	}
	if ((flags = attr_search(attrs, NULL, attr_flags))){
		ret->flags=flags->u.num;
	}
	return ret;
}

void
plugin_init(void) {
	plugin_register_speech_type("winTTS", speech_winTTS_new);
}
