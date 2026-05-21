// Copyright 2016-2026 Vector 35 Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#include <stdexcept>
#include <exception>
#include <utility>

// XXX: Compiled directly into the core for performance reasons
// Will still work fine compiled independently, just at about a
// 50-100% performance penalty due to FFI overhead
#ifdef BINARYNINJACORE_LIBRARY
#include "qualifiedname.h"
#include "type.h"
#include "architecture.h"
#include "binaryview.h"
#include "demangle.h"
#include "unicode.h"
#define BN BinaryNinjaCore
#define _STD_STRING BinaryNinjaCore::string
#define _STD_VECTOR BinaryNinjaCore::vector
#define _STD_SET BinaryNinjaCore::set
#else
#include "binaryninjaapi.h"
#define BN BinaryNinja
#define _STD_STRING std::string
#define _STD_VECTOR std::vector
#define _STD_SET std::set
#endif

#ifdef BINARYNINJACORE_LIBRARY
#include "demangler/gnu3/demangled_type_node.h"
#else
#include "../gnu3/demangled_type_node.h"
#endif

class DemangleException: public std::exception
{
	_STD_STRING m_message;
public:
	DemangleException(_STD_STRING msg="Attempt to read beyond bounds or missing expected character"): m_message(msg){}
	virtual const char* what() const noexcept { return m_message.c_str(); }
};


class Demangle
{
	enum NameType
	{
		NameEmpty,
		NameString,
		NameLookup,
		NameBackref,
		NameTemplate,
		NameConstructor,
		NameDestructor,
		NameRtti,
		NameReturn,
		NameDynamicInitializer,
		NameDynamicAtExitDestructor,
		NameLocalStaticThreadGuard,
		NameLocalVftable,
		NameAnonymousNamespace
	};

	enum FunctionClass
	{
		NoneFunctionClass           = 0,
		PrivateFunctionClass        = 1 << 0,
		ProtectedFunctionClass      = 1 << 1,
		PublicFunctionClass         = 1 << 2,
		GlobalFunctionClass         = 1 << 3,
		StaticFunctionClass         = 1 << 4,
		VirtualFunctionClass        = 1 << 5,
		FriendFunctionClass         = 1 << 6,
		StaticThunkFunctionClass    = 1 << 7,
		VirtualThunkFunctionClass   = 1 << 8,
		VirtualThunkExFunctionClass = 1 << 9,
	};

public:
	struct DemangleContext
	{
		DemangledTypeNode type;
		BNMemberAccess access;
		BNMemberScope scope;
	};

private:
	class Reader
	{
	public:
		Reader(const _STD_STRING& data) : m_ptr(data.c_str()), m_end(data.c_str() + data.size())
		{
			for (const char* p = m_ptr; p < m_end; p++)
				if (*p < 0x20 || *p > 0x7e)
					throw DemangleException();
		}
		void Reset(const _STD_STRING& data)
		{
			m_ptr = data.c_str();
			m_end = data.c_str() + data.size();
			for (const char* p = m_ptr; p < m_end; p++)
				if (*p < 0x20 || *p > 0x7e)
					throw DemangleException();
		}
		bool PeekMatch(const char* str, size_t len) const
		{
			if (len > Length())
				throw DemangleException();
			if (len == 2)
				return m_ptr[0] == str[0] && m_ptr[1] == str[1];
			if (len == 3)
				return m_ptr[0] == str[0] && m_ptr[1] == str[1] && m_ptr[2] == str[2];
			return memcmp(m_ptr, str, len) == 0;
		}
		char PeekAt(size_t offset) const
		{
			if (m_ptr + offset >= m_end)
				throw DemangleException();
			return m_ptr[offset];
		}
		char Peek() const
		{
			if (m_ptr >= m_end)
				throw DemangleException();
			return *m_ptr;
		}
		const char* GetRaw() const { return m_ptr; }
		void SetRaw(const char* p) { m_ptr = p; }
		char Read()
		{
			if (m_ptr >= m_end)
				throw DemangleException();
			return *m_ptr++;
		}
		void Consume(size_t count = 1)
		{
			if (m_ptr + count > m_end)
				throw DemangleException();
			m_ptr += count;
		}
		size_t Length() const { return (size_t)(m_end - m_ptr); }
		_STD_STRING ReadString(size_t count);
		_STD_STRING ReadUntil(char sentinal);
	private:
		const char* m_ptr;
		const char* m_end;
	};

	class BackrefList
	{
	public:
		_STD_VECTOR<DemangledTypeNode::NodeRef> typeList;
		_STD_VECTOR<DemangledNamePart::Ref> nameList;
		_STD_VECTOR<DemangledNamePart::Ref> templateList;
		void Clear() { typeList.clear(); nameList.clear(); templateList.clear(); }
		DemangledTypeNode::NodeRef GetTypeBackrefRef(size_t reference);
		DemangledNamePart::Ref GetNameBackrefRef(size_t reference);
		const DemangledTypeNode& GetTypeBackref(size_t reference);
		const DemangledNamePart& GetNameBackref(size_t reference);
		DemangledTypeNode::NodeRef PushTypeBackref(DemangledTypeNode::NodeRef t);
		DemangledTypeNode::NodeRef PushTypeBackref(const DemangledTypeNode& t);
		DemangledTypeNode::NodeRef PushTypeBackref(DemangledTypeNode&& t);
		DemangledNamePart::Ref PushNameBackref(DemangledNamePart::Ref t);
		DemangledNamePart::Ref PushNameBackref(const DemangledNamePart& t);
		DemangledNamePart::Ref PushNameBackref(DemangledNamePart&& t);
		DemangledNamePart::Ref PushTemplateSpecialization(DemangledNamePart::Ref t);
		DemangledNamePart::Ref PushTemplateSpecialization(const DemangledNamePart& t);
		DemangledNamePart::Ref PushTemplateSpecialization(DemangledNamePart&& t);
	};

	struct BackrefContextSwitch
	{
		BackrefList& active;
		BackrefList saved;

		BackrefContextSwitch(BackrefList& active);
		BackrefContextSwitch(const BackrefContextSwitch&) = delete;
		BackrefContextSwitch& operator=(const BackrefContextSwitch&) = delete;
		~BackrefContextSwitch();

		static void Swap(BackrefList& left, BackrefList& right);
	};

	// Internal name list type - keeps template names structured during parsing.
	using NameList = _STD_VECTOR<DemangledNamePart>;

	static DemangledNamePart MakeNameSegment(const _STD_STRING& s)
	{
		return DemangledNamePart(s);
	}

	static void AppendToLastNameSegment(NameList& nl, const _STD_STRING& suffix)
	{
		if (nl.empty())
			throw DemangleException();
		nl.back() = MakeNameSegment(nl.back().GetString() + suffix);
	}

	static _STD_STRING JoinNameList(const NameList& nl)
	{
		if (nl.empty()) return {};
		if (nl.size() == 1) return nl[0].GetString();
		_STD_STRING out;
		out.reserve(nl.size() * 16);
		out = nl[0].GetString();
		for (size_t i = 1; i < nl.size(); i++)
		{
			out += ':';
			out += ':';
			out += nl[i].GetString();
		}
		return out;
	}

	static StringList FinalizeNameList(const NameList& nl)
	{
		StringList out;
		out.reserve(nl.size());
		for (const auto& n: nl)
			out.push_back(n.GetString());
		return out;
	}

	_STD_STRING m_mangledName; // Owns the string; Reader points into it
	Reader reader;
	BackrefList m_backrefList;
	BN::Architecture* m_arch;
	BN::Ref<BN::Platform> m_platform;
	BN::Ref<BN::BinaryView> m_view;
	NameList m_varName;
	size_t m_templateParamDepth = 0;
	size_t m_nestingDepth = 0;
	class NestingGuard
	{
		Demangle& m_demangler;
	public:
		NestingGuard(Demangle& demangler);
		~NestingGuard();
	};

	NameType GetNameType();
	void RewriteTemplateBackrefName(NameList& typeName, const BackrefList& nameBackrefList) const;
	DemangledTypeNode DemangleReferencedSymbolValue(BackrefList& varList);
	DemangledTypeNode DemangleAutoNonTypeTemplateParam(BackrefList& varList);
	DemangledTypeNode DemangleVarType(BackrefList& varList, bool isReturn, NameList& name,
		bool includeImplicitThis = true, DemangledTypeNode::NodeRef* outTypeBackref = nullptr);
	DemangledTypeNode::NodeRef TryDemangleVarTypeRef(BackrefList& varList, bool isReturn, NameList& name,
		bool includeImplicitThis = true);
	void DemangleNumber(int64_t& num);
	void DemangleChar(char& ch);
	void DemangleWideChar(uint16_t& wch);
	void DemangleModifiers(bool& _const, bool& _volatile, bool& isMember);
	uint8_t DemanglePointerSuffix();
	void DemangleVariableList(_STD_VECTOR<DemangledTypeNode::Param>& paramList, BackrefList& varList, bool typeBackrefs = true);
	void DemangleNameTypeRtti(BNNameType& classFunctionType,
	                          BackrefList& nameBackrefList,
	                          _STD_STRING& out,
	                          _STD_STRING& rttiTypeName);
	void DemangleTypeNameLookup(_STD_STRING& out, BNNameType& functionType);
	void DemangleNameTypeString(_STD_STRING& out);
	void DemangleName(NameList& nameList,
	                  BNNameType& classFunctionType,
	                  BackrefList& nameBackrefList,
	                  bool typeNameContext = false);
	BNCallingConventionName DemangleCallingConvention();
	void ConsumeExtendedModifierPrefix();
	DemangledTypeNode DemangleFunction(BNNameType classFunctionType, bool pointerSuffix, BackrefList& varList,
		int funcClass = NoneFunctionClass, bool includeImplicitThis = true);
	DemangledTypeNode DemangleData(BackrefList& varList);
	void DemangleNameTypeRtti(BNNameType& classFunctionType,
	                          BackrefList& nameBackrefList,
	                          _STD_STRING& out);
	DemangledTypeNode DemangleVTable(BackrefList& nameBackrefList);
	DemangledTypeNode DemanagleRTTI(BNNameType classFunctionType);
	DemangledNamePart DemangleTemplateInstantiationNameInLocalContext(BackrefList& nameBackrefList);
	DemangledNamePart DemangleTemplateInstantiationName(BackrefList& nameBackrefList);
	void DemangleTemplateParams(_STD_VECTOR<DemangledTypeNode::Param>& params, BackrefList& nameBackrefList, DemangledNamePart& out);
	DemangledNamePart DemangleUnqualifiedSymbolName(NameList& nameList, BackrefList& nameBackrefList,
		BNNameType& classFunctionType, bool& backrefEligible);
	DemangledTypeNode DemangleString();
	DemangledTypeNode DemangleTypeInfoName();
	DemangleContext DemangleDynamicInitFini(bool isDtor, BackrefList& backrefList);
	DemangleContext DemangleSymbol(BackrefList& backrefList);
	std::pair<BN::Ref<BN::Type>, BN::QualifiedName> Finalize(BN::BinaryView* view);

public:
	Demangle(BN::Architecture* arch, const _STD_STRING& mangledName);
	Demangle(BN::Ref<BN::BinaryView> view, const _STD_STRING& mangledName);
	Demangle(BN::Ref<BN::Platform> platform, const _STD_STRING& mangledName);
	void Reset(BN::Architecture* arch, const _STD_STRING& mangledName);
	DemangleContext DemangleSymbol();
	std::pair<BN::Ref<BN::Type>, BN::QualifiedName> Finalize();

	// Be careful not to accidentally implicitly cast a BinaryView* to a bool
	static bool DemangleMS(BN::Architecture* arch, const _STD_STRING& mangledName, BN::Ref<BN::Type>& outType,
	                       BN::QualifiedName& outVarName, const BN::Ref<BN::BinaryView>& view);
	static bool DemangleMS(BN::Architecture* arch, const _STD_STRING& mangledName, BN::Ref<BN::Type>& outType,
	                       BN::QualifiedName& outVarName, BN::BinaryView* view);
	static bool DemangleMS(BN::Platform* platform, const _STD_STRING& mangledName, BN::Ref<BN::Type>& outType,
	                       BN::QualifiedName& outVarName);
	static bool DemangleMS(BN::Architecture* arch, const _STD_STRING& mangledName, BN::Ref<BN::Type>& outType,
	                       BN::QualifiedName& outVarName);

	static bool DemangleMS(const _STD_STRING& mangledName, BN::Ref<BN::Type>& outType,
	                       BN::QualifiedName& outVarName, const BN::Ref<BN::BinaryView>& view);
	static bool DemangleMS(const _STD_STRING& mangledName, BN::Ref<BN::Type>& outType,
	                       BN::QualifiedName& outVarName, BN::BinaryView* view);
};
