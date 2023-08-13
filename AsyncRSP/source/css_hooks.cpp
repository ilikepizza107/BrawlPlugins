#include <CXCompression.h>
#include <OS/OSCache.h>
#include <OS/OSError.h>
#include <gf/gf_archive.h>
#include <memory.h>
#include <modules.h>
#include <mu/mu_menu.h>
#include <mu/mu_sel_char_player_area.h>
#include <nw4r/g3d/g3d_resfile.h>
#include <sy_core.h>
#include <types.h>
#include <vector.h>

#include "sel_char_load_thread.h"

using namespace nw4r::g3d;

namespace CSSHooks {

    extern gfArchive* selCharArchive;
    selCharLoadThread* threads[4];

    void createThreads(muSelCharPlayerArea* area)
    {
        selCharLoadThread* thread = new (Heaps::Thread) selCharLoadThread(area);
        threads[area->areaIdx] = thread;
        thread->start();
    }

    ResFile* getCharPicTexResFile(register muSelCharPlayerArea* area, u32 charKind)
    {
        selCharLoadThread* thread = threads[area->areaIdx];

        // if thread has loaded the data
        if (!thread->m_dataReady)
        {
            // Handles conversions for poketrio and special slots
            int id = muMenu::exchangeMuSelchkind2MuStockchkind(charKind);
            id = muMenu::getStockFrameID(id);

            // check if CSP exists in archive first. We do this check
            // when thread hasn't loaded data because this function
            // will get called again by the load thread when it's ready
            // and we don't want to check the archive a 2nd time
            void* data = selCharArchive->getData(Data_Type_Misc, id, 0xfffe);
            if (data)
            {
                CXUncompressLZ(data, (void*)area->charPicData);
            }
            else
            {
                // if it does not, request thread
                // to load RSP and return
                asm {
                    lwz r3, 0x404(area);
                    lwz r4, 0xC0(area);
                    lwz r4, 0x10(r4);
                    lwz r12, 0x0(r3);
                    lwz r12, 0x3c(r12);
                    mtctr r12;
                    bctrl;
                }
                thread->requestLoad(charKind);
                return &area->charPicRes;
            }
        }

        ResFile* resFile = &area->charPicRes;

        // flush cache
        DCFlushRange(area->charPicData, 0x40000);

        // set ResFile to point to filedata
        area->charPicRes = (ResFile)area->charPicData;

        // init resFile and return
        ResFile::Init(resFile);

        // to ensure we load more than just
        // the first hovered character
        thread->reset();

        return resFile;
    }

    muSelCharPlayerArea* (*_destroyPlayerAreas)(void*, int);
    muSelCharPlayerArea* destroyPlayerAreas(muSelCharPlayerArea* object, int external)
    {
        // destroy our load thread
        delete threads[object->areaIdx];
        threads[object->areaIdx] = 0;

        // call base function first
        muSelCharPlayerArea* ret = _destroyPlayerAreas(object, external);

        return ret;
    }

    void InstallHooks()
    {
        // hook to load portraits from RSPs
        SyringeCore::syReplaceFuncRel(0x1107c,
                                      reinterpret_cast<void*>(getCharPicTexResFile),
                                      NULL,
                                      Modules::SORA_MENU_SEL_CHAR);

        // hook to clean up our mess when unloading CSS
        SyringeCore::syReplaceFunc(0x806937bc,
                                   reinterpret_cast<void*>(destroyPlayerAreas),
                                   (void**)&_destroyPlayerAreas,
                                   Modules::SORA_MENU_SEL_CHAR);

        // hook to create threads when booting the CSS
        SyringeCore::syInlineHook(0x80685DE8, reinterpret_cast<void*>(createThreads), Modules::SORA_MENU_SEL_CHAR);
    }
} // namespace CSSHooks
