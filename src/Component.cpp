/*
 * Originally from the CitizenFX project - http://citizen.re/
 *
 * Copyright (c) 2017-2020 the CitizenFX Collective
 * Licensed under the GNU Lesser General Public License v2.0
 * https://github.com/citizenfx/fivem/blob/master/code/components/citizen-scripting-lua/src/Component.cpp
 */

#include "../include/CppScriptRuntime.h"

class EXPORTED_TYPE ComponentInstance : public OMComponentBase<Component>
{
public:
        virtual bool Initialize();

        virtual bool DoGameLoad(void* module);

        virtual bool Shutdown();
};

bool ComponentInstance::Initialize()
{
        InitFunctionBase::RunAll();

        return true;
}

bool ComponentInstance::DoGameLoad(void* module)
{
        HookFunction::RunAll();

        return true;
}

bool ComponentInstance::Shutdown()
{
        return true;
}

extern "C" DLL_EXPORT Component* CreateComponent()
{
        return new ComponentInstance();
}

OMComponentBaseImpl* OMComponentBaseImpl::ms_instance;
