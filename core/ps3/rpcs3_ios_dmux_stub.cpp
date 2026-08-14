#include "stdafx.h"

#include "Emu/Cell/Modules/cellDmux.h"
#include "Emu/Cell/PPUModule.h"

// iOS currently builds RPCS3 in headless mode without FFmpeg/PAMF parsing.
// Keep the shared cellDmux module linkable, but fail media-demux requests
// cleanly instead of leaving a null core-operations table behind.

static error_code vshift_dmux_query_attr(ppu_thread&, vm::cptr<void>, vm::ptr<CellDmuxPamfAttr>)
{
	return CELL_DMUX_ERROR_FATAL;
}

static error_code vshift_dmux_open(ppu_thread&, vm::cptr<void>, vm::cptr<CellDmuxResource>, vm::cptr<CellDmuxResourceSpurs>, vm::cptr<DmuxCb<DmuxNotifyDemuxDone>>, vm::cptr<DmuxCb<DmuxNotifyProgEndCode>>, vm::cptr<DmuxCb<DmuxNotifyFatalErr>>, vm::pptr<void>)
{
	return CELL_DMUX_ERROR_FATAL;
}

static error_code vshift_dmux_close(ppu_thread&, vm::ptr<void>)
{
	return CELL_DMUX_ERROR_FATAL;
}

static error_code vshift_dmux_reset_stream(ppu_thread&, vm::ptr<void>)
{
	return CELL_DMUX_ERROR_FATAL;
}

static error_code vshift_dmux_create_thread(ppu_thread&, vm::ptr<void>)
{
	return CELL_DMUX_ERROR_FATAL;
}

static error_code vshift_dmux_join_thread(ppu_thread&, vm::ptr<void>)
{
	return CELL_DMUX_ERROR_FATAL;
}

static error_code vshift_dmux_set_stream(ppu_thread&, vm::ptr<void>, vm::cptr<void>, u32, b8, u64)
{
	return CELL_DMUX_ERROR_FATAL;
}

static error_code vshift_dmux_release_au(ppu_thread&, vm::ptr<void>, vm::ptr<void>, u32)
{
	return CELL_DMUX_ERROR_FATAL;
}

static error_code vshift_dmux_query_es_attr(ppu_thread&, vm::cptr<void>, vm::cptr<void>, vm::ptr<CellDmuxPamfEsAttr>)
{
	return CELL_DMUX_ERROR_FATAL;
}

static error_code vshift_dmux_enable_es(ppu_thread&, vm::ptr<void>, vm::cptr<void>, vm::cptr<CellDmuxEsResource>, vm::cptr<DmuxCb<DmuxEsNotifyAuFound>>, vm::cptr<DmuxCb<DmuxEsNotifyFlushDone>>, vm::cptr<void>, vm::pptr<void>)
{
	return CELL_DMUX_ERROR_FATAL;
}

static error_code vshift_dmux_disable_es(ppu_thread&, vm::ptr<void>)
{
	return CELL_DMUX_ERROR_FATAL;
}

static error_code vshift_dmux_flush_es(ppu_thread&, vm::ptr<void>)
{
	return CELL_DMUX_ERROR_FATAL;
}

static error_code vshift_dmux_reset_es(ppu_thread&, vm::ptr<void>)
{
	return CELL_DMUX_ERROR_FATAL;
}

static error_code vshift_dmux_reset_stream_and_wait_done(ppu_thread&, vm::ptr<void>)
{
	return CELL_DMUX_ERROR_FATAL;
}

static vm::gvar<CellDmuxCoreOps> g_vshift_cell_dmux_core_ops_pamf;
static vm::gvar<CellDmuxCoreOps> g_vshift_cell_dmux_core_ops_raw_es;

template <bool raw_es>
static void vshift_init_dmux_ops(const vm::gvar<CellDmuxCoreOps>& var)
{
	var->queryAttr.set(g_fxo->get<ppu_function_manager>().func_addr(FIND_FUNC(vshift_dmux_query_attr)));
	var->open.set(g_fxo->get<ppu_function_manager>().func_addr(FIND_FUNC(vshift_dmux_open)));
	var->close.set(g_fxo->get<ppu_function_manager>().func_addr(FIND_FUNC(vshift_dmux_close)));
	var->resetStream.set(g_fxo->get<ppu_function_manager>().func_addr(FIND_FUNC(vshift_dmux_reset_stream)));
	var->createThread.set(g_fxo->get<ppu_function_manager>().func_addr(FIND_FUNC(vshift_dmux_create_thread)));
	var->joinThread.set(g_fxo->get<ppu_function_manager>().func_addr(FIND_FUNC(vshift_dmux_join_thread)));
	var->setStream.set(g_fxo->get<ppu_function_manager>().func_addr(FIND_FUNC(vshift_dmux_set_stream)));
	var->releaseAu.set(g_fxo->get<ppu_function_manager>().func_addr(FIND_FUNC(vshift_dmux_release_au)));
	var->queryEsAttr.set(g_fxo->get<ppu_function_manager>().func_addr(FIND_FUNC(vshift_dmux_query_es_attr)));
	var->enableEs.set(g_fxo->get<ppu_function_manager>().func_addr(FIND_FUNC(vshift_dmux_enable_es)));
	var->disableEs.set(g_fxo->get<ppu_function_manager>().func_addr(FIND_FUNC(vshift_dmux_disable_es)));
	var->flushEs.set(g_fxo->get<ppu_function_manager>().func_addr(FIND_FUNC(vshift_dmux_flush_es)));
	var->resetEs.set(g_fxo->get<ppu_function_manager>().func_addr(FIND_FUNC(vshift_dmux_reset_es)));
	var->resetStreamAndWaitDone.set(g_fxo->get<ppu_function_manager>().func_addr(FIND_FUNC(vshift_dmux_reset_stream_and_wait_done)));
}

DECLARE(ppu_module_manager::cellDmuxPamf)("cellDmuxPamf", []
{
	REG_VNID(cellDmuxPamf, 0x28b2b7b2, g_vshift_cell_dmux_core_ops_pamf).init = []
	{
		vshift_init_dmux_ops<false>(g_vshift_cell_dmux_core_ops_pamf);
	};
	REG_VNID(cellDmuxPamf, 0x9728a0e9, g_vshift_cell_dmux_core_ops_raw_es).init = []
	{
		vshift_init_dmux_ops<true>(g_vshift_cell_dmux_core_ops_raw_es);
	};

	REG_HIDDEN_FUNC(vshift_dmux_query_attr);
	REG_HIDDEN_FUNC(vshift_dmux_open);
	REG_HIDDEN_FUNC(vshift_dmux_close);
	REG_HIDDEN_FUNC(vshift_dmux_reset_stream);
	REG_HIDDEN_FUNC(vshift_dmux_create_thread);
	REG_HIDDEN_FUNC(vshift_dmux_join_thread);
	REG_HIDDEN_FUNC(vshift_dmux_set_stream);
	REG_HIDDEN_FUNC(vshift_dmux_release_au);
	REG_HIDDEN_FUNC(vshift_dmux_query_es_attr);
	REG_HIDDEN_FUNC(vshift_dmux_enable_es);
	REG_HIDDEN_FUNC(vshift_dmux_disable_es);
	REG_HIDDEN_FUNC(vshift_dmux_flush_es);
	REG_HIDDEN_FUNC(vshift_dmux_reset_es);
	REG_HIDDEN_FUNC(vshift_dmux_reset_stream_and_wait_done);
});
