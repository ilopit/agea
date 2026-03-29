#include "vfs/vfs_state.h"

#include <vfs/vfs.h>

#include <global_state/global_state.h>

void
kryga::state_mutator__vfs::set(gs::state& es)
{
    es.m_vfs = es.create_box<vfs::virtual_file_system>("vfs");
}
