/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2009, 2015-2016 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "FunctionPrototype.h"

#include "BuiltinExecutables.h"
#include "BuiltinNames.h"
#include "Error.h"
#include "GetterSetter.h"
#include "JSAsyncFunction.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSStringInlines.h"
#include "Lexer.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(FunctionPrototype);

const ClassInfo FunctionPrototype::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(FunctionPrototype) };

static EncodedJSValue JSC_HOST_CALL functionProtoFuncToString(ExecState*);

// ECMA 15.3.4
static EncodedJSValue JSC_HOST_CALL callFunctionPrototype(ExecState*)
{
    return JSValue::encode(jsUndefined());
}

FunctionPrototype::FunctionPrototype(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callFunctionPrototype, nullptr)
{
}

void FunctionPrototype::finishCreation(VM& vm, const String& name)
{
    Base::finishCreation(vm, name);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

void FunctionPrototype::addFunctionProperties(ExecState* exec, JSGlobalObject* globalObject, JSFunction** callFunction, JSFunction** applyFunction, JSFunction** hasInstanceSymbolFunction)
{
    VM& vm = exec->vm();

    JSFunction* toStringFunction = JSFunction::create(vm, globalObject, 0, vm.propertyNames->toString.string(), functionProtoFuncToString);
    putDirectWithoutTransition(vm, vm.propertyNames->toString, toStringFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));

    *applyFunction = putDirectBuiltinFunctionWithoutTransition(vm, globalObject, vm.propertyNames->builtinNames().applyPublicName(), functionPrototypeApplyCodeGenerator(vm).value(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    *callFunction = putDirectBuiltinFunctionWithoutTransition(vm, globalObject, vm.propertyNames->builtinNames().callPublicName(), functionPrototypeCallCodeGenerator(vm).value(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectBuiltinFunctionWithoutTransition(vm, globalObject, vm.propertyNames->bind, functionPrototypeBindCodeGenerator(vm).value(), static_cast<unsigned>(PropertyAttribute::DontEnum));

    *hasInstanceSymbolFunction = JSFunction::create(vm, functionPrototypeSymbolHasInstanceCodeGenerator(vm).value(), globalObject);
    putDirectWithoutTransition(vm, vm.propertyNames->hasInstanceSymbol, *hasInstanceSymbolFunction, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}
    
void FunctionPrototype::initRestrictedProperties(ExecState* exec, JSGlobalObject* globalObject)
{
    VM& vm = exec->vm();
    GetterSetter* errorGetterSetter = globalObject->throwTypeErrorArgumentsCalleeAndCallerGetterSetter();
    putDirectAccessor(exec, vm.propertyNames->caller, errorGetterSetter, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    putDirectAccessor(exec, vm.propertyNames->arguments, errorGetterSetter, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
}

EncodedJSValue JSC_HOST_CALL functionProtoFuncToString(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    if (thisValue.inherits(vm, JSFunction::info())) {
        JSFunction* function = jsCast<JSFunction*>(thisValue);
        if (function->isHostOrBuiltinFunction()) {
            scope.release();
            return JSValue::encode(jsMakeNontrivialString(exec, "function ", function->name(vm), "() {\n    [native code]\n}"));
        }

        FunctionExecutable* executable = function->jsExecutable();
        if (executable->isClass()) {
            StringView classSource = executable->classSource().view();
            return JSValue::encode(jsString(exec, classSource.toStringWithoutCopying()));
        }

        if (thisValue.inherits(vm, JSAsyncFunction::info())) {
            String functionHeader = executable->isArrowFunction() ? "async " : "async function ";

            StringView source = executable->source().provider()->getRange(
                executable->parametersStartOffset(),
                executable->parametersStartOffset() + executable->source().length());
            return JSValue::encode(jsMakeNontrivialString(exec, functionHeader, function->name(vm), source));
        }

        String functionHeader = executable->isArrowFunction() ? "" : "function ";
        
        StringView source = executable->source().provider()->getRange(
            executable->parametersStartOffset(),
            executable->parametersStartOffset() + executable->source().length());
        scope.release();
        return JSValue::encode(jsMakeNontrivialString(exec, functionHeader, function->name(vm), source));
    }

    if (thisValue.inherits(vm, InternalFunction::info())) {
        InternalFunction* function = asInternalFunction(thisValue);
        scope.release();
        return JSValue::encode(jsMakeNontrivialString(exec, "function ", function->name(), "() {\n    [native code]\n}"));
    }

    if (thisValue.isObject()) {
        JSObject* object = asObject(thisValue);
        if (object->inlineTypeFlags() & TypeOfShouldCallGetCallData) {
            CallData callData;
            if (object->methodTable(vm)->getCallData(object, callData) != CallType::None) {
                if (auto* classInfo = object->classInfo(vm)) {
                    scope.release();
                    return JSValue::encode(jsMakeNontrivialString(exec, "function ", classInfo->className, "() {\n    [native code]\n}"));
                }
            }
        }
    }

    return throwVMTypeError(exec, scope);
}

} // namespace JSC