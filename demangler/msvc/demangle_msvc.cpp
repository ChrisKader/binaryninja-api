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

// Includes snippets from LLVM, which is under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.

#include "demangle_msvc.h"
#include <memory>


#ifdef BINARYNINJACORE_LIBRARY
using namespace BinaryNinjaCore;
#else
using namespace BinaryNinja;
using namespace std;
#endif


static constexpr size_t MAX_DEMANGLE_NESTING_DEPTH = 1024;

static int64_t SignExtendInt32(int64_t value)
{
	uint64_t lowBits = static_cast<uint64_t>(value) & 0xffffffffULL;
	if ((lowBits & 0x80000000ULL) != 0)
		return static_cast<int64_t>(lowBits) - 0x100000000LL;
	return static_cast<int64_t>(lowBits);
}

// Define MSVC_DEMANGLE_DEBUG to enable trace logging
#ifdef MSVC_DEMANGLE_DEBUG
#define MSVC_TRACE(...) LogTrace(__VA_ARGS__)
#else
#define MSVC_TRACE(...) do {} while(0)
#endif

string Demangle::Reader::ReadString(size_t count)
{
	if (m_ptr + count >= m_end)
		throw DemangleException();
	string out(m_ptr, count);
	m_ptr += count + 1; // skip count chars + sentinel
	return out;
}


string Demangle::Reader::ReadUntil(char sentinal)
{
	const char* found = (const char*)memchr(m_ptr, sentinal, m_end - m_ptr);
	if (!found)
		throw DemangleException();
	size_t count = found - m_ptr;
	return ReadString(count);
}


DemangledTypeNode::NodeRef Demangle::BackrefList::GetTypeBackrefRef(size_t reference)
{
	if (reference < typeList.size() && typeList[reference])
		return typeList[reference];
	throw DemangleException(string("Backref too large " + std::to_string(reference)));
}


DemangledNamePart::Ref Demangle::BackrefList::GetNameBackrefRef(size_t reference)
{
	if (reference < nameList.size() && nameList[reference])
		return nameList[reference];
	MSVC_TRACE("type: %p - Backref too large: %zu/%zu\n", this, nameList.size(), reference);
	throw DemangleException(string("Backref too large " + std::to_string(reference)));
}


const DemangledTypeNode& Demangle::BackrefList::GetTypeBackref(size_t reference)
{
	return *GetTypeBackrefRef(reference);
}


const DemangledNamePart& Demangle::BackrefList::GetNameBackref(size_t reference)
{
	return *GetNameBackrefRef(reference);
}


DemangledTypeNode::NodeRef Demangle::BackrefList::PushTypeBackref(DemangledTypeNode::NodeRef t)
{
	if (!t)
		return nullptr;
	if (typeList.size() > 9)
		return nullptr;
	typeList.push_back(t);
	return t;
}


DemangledTypeNode::NodeRef Demangle::BackrefList::PushTypeBackref(const DemangledTypeNode& t)
{
	if (typeList.size() <= 9)
		return PushTypeBackref(DemangledTypeNode::CreateSharedCopy(t));
	return nullptr;
}


DemangledTypeNode::NodeRef Demangle::BackrefList::PushTypeBackref(DemangledTypeNode&& t)
{
	if (typeList.size() <= 9)
		return PushTypeBackref(DemangledTypeNode::CreateShared(std::move(t)));
	return nullptr;
}


DemangledNamePart::Ref Demangle::BackrefList::PushNameBackref(DemangledNamePart::Ref t)
{
	if (!t)
		return nullptr;
	MSVC_TRACE("this: %p - Backref: %zu\n", this, nameList.size());
	for (const auto& name : nameList)
		if (name && ((name == t) || name->IsStructurallyEqual(*t)))
			return name;
	if (nameList.size() <= 9)
	{
		nameList.push_back(t);
		return t;
	}
	return nullptr;
}


DemangledNamePart::Ref Demangle::BackrefList::PushNameBackref(const DemangledNamePart& t)
{
	MSVC_TRACE("this: %p - Backref: %zu\n", this, nameList.size());
	for (const auto& name : nameList)
		if (name && name->IsStructurallyEqual(t))
			return name;
	if (nameList.size() <= 9)
	{
		auto ref = DemangledNamePart::CreateSharedCopy(t);
		nameList.push_back(ref);
		return ref;
	}
	return nullptr;
}


DemangledNamePart::Ref Demangle::BackrefList::PushNameBackref(DemangledNamePart&& t)
{
	MSVC_TRACE("this: %p - Backref: %zu\n", this, nameList.size());
	for (const auto& name : nameList)
		if (name && name->IsStructurallyEqual(t))
			return name;
	if (nameList.size() <= 9)
	{
		auto ref = DemangledNamePart::CreateShared(std::move(t));
		nameList.push_back(ref);
		return ref;
	}
	return nullptr;
}


DemangledNamePart::Ref Demangle::BackrefList::PushTemplateSpecialization(DemangledNamePart::Ref t)
{
	if (!t)
		return nullptr;
	templateList.push_back(t);
	return t;
}


DemangledNamePart::Ref Demangle::BackrefList::PushTemplateSpecialization(const DemangledNamePart& t)
{
	return PushTemplateSpecialization(DemangledNamePart::CreateSharedCopy(t));
}


DemangledNamePart::Ref Demangle::BackrefList::PushTemplateSpecialization(DemangledNamePart&& t)
{
	return PushTemplateSpecialization(DemangledNamePart::CreateShared(std::move(t)));
}


Demangle::BackrefContextSwitch::BackrefContextSwitch(BackrefList& active): active(active)
{
	Swap(active, saved);
}


Demangle::BackrefContextSwitch::~BackrefContextSwitch()
{
	Swap(active, saved);
}


void Demangle::BackrefContextSwitch::Swap(BackrefList& left, BackrefList& right)
{
	std::swap(left.typeList, right.typeList);
	std::swap(left.nameList, right.nameList);
	std::swap(left.templateList, right.templateList);
}



Demangle::Demangle(Architecture* arch, const string& mangledName) :
	m_mangledName(mangledName),
	reader(m_mangledName),
	m_arch(arch),
	m_platform(nullptr),
	m_view(nullptr)
{
}


Demangle::Demangle(Ref<Platform> platform, const string& mangledName) :
	m_mangledName(mangledName),
	reader(m_mangledName),
	m_arch(nullptr),
	m_platform(platform),
	m_view(nullptr)
{
}


Demangle::Demangle(Ref<BinaryView> view, const string& mangledName) :
	m_mangledName(mangledName),
	reader(m_mangledName),
	m_arch(nullptr),
	m_platform(nullptr),
	m_view(view)
{
}


Demangle::NestingGuard::NestingGuard(Demangle& demangler) : m_demangler(demangler)
{
	m_demangler.m_nestingDepth++;
	if (m_demangler.m_nestingDepth > MAX_DEMANGLE_NESTING_DEPTH)
	{
		m_demangler.m_nestingDepth--;
		throw DemangleException("Detected adversarial mangled string");
	}
}


Demangle::NestingGuard::~NestingGuard()
{
	m_demangler.m_nestingDepth--;
}


void Demangle::Reset(Architecture* arch, const string& mangledName)
{
	m_mangledName = mangledName;
	reader.Reset(m_mangledName);
	m_backrefList.Clear();
	m_arch = arch;
	m_platform = nullptr;
	m_view = nullptr;
	m_varName.clear();
	m_templateParamDepth = 0;
	m_nestingDepth = 0;
}


void Demangle::RewriteTemplateBackrefName(NameList& typeName, const BackrefList& nameBackrefList) const
{
	if (typeName.empty())
		return;

	DemangledNamePart& baseName = typeName.back();
	if (baseName.HasTemplateArguments())
		return;
	string base = baseName.GetBase();

	for (auto it = nameBackrefList.templateList.rbegin(); it != nameBackrefList.templateList.rend(); ++it)
	{
		if (!*it)
			continue;
		const DemangledNamePart& candidate = **it;
		if (!candidate.HasTemplateArguments())
			continue;
		if (candidate.GetBase() != base)
			continue;
		baseName = candidate;
		return;
	}
}

DemangledTypeNode Demangle::DemangleReferencedSymbolValue(BackrefList& varList)
{
	// Template argument backrefs are scoped. A referenced symbol nested inside
	// a non-type template argument can see backrefs already introduced by the
	// surrounding template argument list, but any backrefs created while parsing
	// that nested symbol must not leak back into the outer template list.
	BackrefList symbolBackrefs = varList;
	NameList savedVarName = m_varName;

	try
	{
		auto context = DemangleSymbol(symbolBackrefs);
		string value = "&" + context.type.GetTypeAndName(FinalizeNameList(m_varName));
		m_varName = std::move(savedVarName);

		return DemangledTypeNode::NamedType(UnknownNamedTypeClass, StringList{value});
	}
	catch (const DemangleException&)
	{
		m_varName = std::move(savedVarName);
		throw;
	}
}


DemangledTypeNode Demangle::DemangleAutoNonTypeTemplateParam(BackrefList& varList)
{
	if (reader.Peek() == '0')
	{
		reader.Consume();
		int64_t value;
		DemangleNumber(value);
		return DemangledTypeNode::NamedType(UnknownNamedTypeClass, StringList{to_string(value)});
	}
	if (reader.Peek() == '1')
	{
		reader.Consume();
		return DemangleReferencedSymbolValue(varList);
	}
	throw DemangleException();
}


DemangledTypeNode Demangle::DemangleVarType(BackrefList& varList, bool isReturn, NameList& name,
	bool includeImplicitThis, DemangledTypeNode::NodeRef* outTypeBackref)
{
	NestingGuard nestingGuard(*this);
	MSVC_TRACE("%s: '%s' - %lu\n", __FUNCTION__, reader.GetRaw(), varList.nameList.size());
	if (outTypeBackref)
		*outTypeBackref = nullptr;
	auto recordTypeBackref = [&](const DemangledTypeNode& type) -> DemangledTypeNode::NodeRef {
		if (isReturn)
			return nullptr;
		auto ref = varList.PushTypeBackref(type);
		if (outTypeBackref)
			*outTypeBackref = ref;
		return ref;
	};
	DemangledTypeNode newType;
	bool _const = false, _volatile = false, isMember = false;
	BNReferenceType refType;
	BNTypeClass typeClass = IntegerTypeClass;
	BNStructureVariant structType;
	NameList varName;
	NameList typeName;
	BNNameType classFunctionType;
	size_t width = 0;
	bool _enumSigned = false;
	char elm = reader.Read();
	switch (elm)
	{
	case 'A':
		typeClass = PointerTypeClass;
		refType = ReferenceReferenceType;
		_const = false;
		_volatile = false;
		break;
	case 'B':
		typeClass = PointerTypeClass;
		refType = ReferenceReferenceType;
		_const = false;
		_volatile = true;
		break;
	case 'C': return DemangledTypeNode::IntegerType(1, true, "signed char");
	case 'D': return DemangledTypeNode::IntegerType(1, true);
	case 'E': return DemangledTypeNode::IntegerType(1, false);
	case 'F': return DemangledTypeNode::IntegerType(2, true);
	case 'G': return DemangledTypeNode::IntegerType(2, false);
	case 'H': return DemangledTypeNode::IntegerType(4, true);
	case 'I': return DemangledTypeNode::IntegerType(4, false);
	case 'J': return DemangledTypeNode::IntegerType(4, true, "long");
	case 'K': return DemangledTypeNode::IntegerType(4, false, "unsigned long");
	case 'M': return DemangledTypeNode::FloatType(4);
	case 'N': return DemangledTypeNode::FloatType(8);
	case 'O': return DemangledTypeNode::FloatType(10, "long double");
	case 'P': // *
		typeClass = PointerTypeClass;
		refType = PointerReferenceType;
		_const = false;
		_volatile = false;
		break;
	case 'Q': // const *
		typeClass = PointerTypeClass;
		refType = PointerReferenceType;
		_const = true;
		_volatile = false;
		break;
	case 'R': // volatile *
		typeClass = PointerTypeClass;
		refType = PointerReferenceType;
		_const = false;
		_volatile = true;
		break;
	case 'S': // const volatile *
		typeClass = PointerTypeClass;
		refType = PointerReferenceType;
		_const = true;
		_volatile = true;
		break;
	case 'T': typeClass = StructureTypeClass; structType = UnionStructureType;  break;
	case 'U': typeClass = StructureTypeClass; structType = StructStructureType; break;
	case 'V': typeClass = StructureTypeClass; structType = ClassStructureType;  break;
	case 'W':
		typeClass = EnumerationTypeClass;
		switch (reader.Read())
		{
		case '0': width = 1; _enumSigned = true;  break;
		case '1': width = 1; _enumSigned = false; break;
		case '2': width = 2; _enumSigned = true;  break;
		case '3': width = 2; _enumSigned = false; break;
		case '4': width = 4; _enumSigned = true;  break;
		case '5': width = 4; _enumSigned = false; break;
		case '6': width = 4; _enumSigned = true;  break;
		case '7': width = 4; _enumSigned = false; break;
		default: throw DemangleException();
		}
		break;
	case 'X': return DemangledTypeNode::VoidType(); break;
	case 'Y':
	{
		// Multi-dimensional array type: Y<ndims><dim1><dim2>...@<elemtype>
		int64_t nDimensions;
		DemangleNumber(nDimensions);
		_STD_VECTOR<uint64_t> elementList;
		while (nDimensions--)
		{
			int64_t element = 0;
			DemangleNumber(element);
			elementList.push_back(element);
		}
		NameList arrayName;
		newType = DemangleVarType(varList, false, arrayName);
		for (auto i = elementList.rbegin(); i != elementList.rend(); i++)
		{
			newType = DemangledTypeNode::ArrayType(std::move(newType), *i);
		}
		recordTypeBackref(newType);
		return newType;
	}
	case 'Z': return DemangledTypeNode::VarArgsType();
	case '?':
	{
		if (reader.Peek() >= '0' && reader.Peek() <= '9')
		{
			size_t reference = reader.Read() - '0';
			try
			{
				auto ref = varList.GetTypeBackrefRef(reference);
				if (outTypeBackref)
					*outTypeBackref = ref;
				return *ref;
			}
			catch (const DemangleException&)
			{
				if (reference == 2)
					return DemangledTypeNode::NamedType(UnknownNamedTypeClass, vector<string>{"auto"});
				throw;
			}
		}
		if (reader.Peek() != '<')
			throw DemangleException();

		string placeholder = reader.ReadUntil('@');
		if (reader.Peek() == '@')
			reader.Consume();
		if (placeholder == "<auto>")
			return DemangledTypeNode::NamedType(UnknownNamedTypeClass, vector<string>{"auto"});
		if (placeholder == "<decltype-auto>")
			return DemangledTypeNode::NamedType(UnknownNamedTypeClass, vector<string>{"decltype(auto)"});
		return DemangledTypeNode::NamedType(UnknownNamedTypeClass, vector<string>{placeholder});
	}
	case '_':
		switch (reader.Read())
		{
		case 'D': newType = DemangledTypeNode::IntegerType(1, true); break;
		case 'E': newType = DemangledTypeNode::IntegerType(1, false); break;
		case 'F': newType = DemangledTypeNode::IntegerType(2, true); break;
		case 'G': newType = DemangledTypeNode::IntegerType(2, false); break;
		case 'H': newType = DemangledTypeNode::IntegerType(4, true); break;
		case 'I': newType = DemangledTypeNode::IntegerType(4, false); break;
		case 'J': newType = DemangledTypeNode::IntegerType(8, true); break;
		case 'K': newType = DemangledTypeNode::IntegerType(8, false); break;
		case 'L': newType = DemangledTypeNode::IntegerType(16, true); break;
		case 'M': newType = DemangledTypeNode::IntegerType(16, false); break;
		case 'N': newType = DemangledTypeNode::BoolType(); break;
		case 'O':
		{
			NameList name;
			auto childType = DemangleVarType(varList, false, name);
			newType = DemangledTypeNode::ArrayType(std::move(childType), 0);
			break;
		}
		case 'S': newType = DemangledTypeNode::WideCharType(2, "char16_t"); break;
		case 'U': newType = DemangledTypeNode::WideCharType(4, "char32_t"); break;
		case 'W': newType = DemangledTypeNode::WideCharType(2, "wchar_t"); break;
		// `_P` (auto) and `_T` (decltype(auto)) are placeholder return-type
		// encodings. For normal source code they are deduced at the function
		// definition and mangled as the deduced type — you will not see `_P`
		// or `_T` from something like `auto foo() { return 0; }` (that becomes
		// `?foo@@YAHXZ`). They do appear in compiler-emitted symbols for
		// function templates whose declared return type is literally `auto`
		// or `decltype(auto)` and which are mangled before/without deduction
		// settling on a concrete type — e.g. `??$seq@HX@llvm@@YA?A_PH@Z`
		// (llvm::seq) or `??$_Get_unwrapped@...@std@@YA?A_T...@Z`. Handle
		// them as named-type placeholders so downstream type consumers get
		// something sensible (rather than a `<FAILED>` demangle) even though
		// the underlying type is not expressible as a Binary Ninja Type.
		case 'P': newType = DemangledTypeNode::NamedType(UnknownNamedTypeClass, StringList{"auto"}); break;
		case 'Q': newType = DemangledTypeNode::IntegerType(1, true, "char8_t"); break; // C++20 char8_t
		case 'T': newType = DemangledTypeNode::NamedType(UnknownNamedTypeClass, StringList{"decltype(auto)"}); break;
		// NOTE: `_X` and `_Y` were previously mapped to coclass/cointerface
		// here, but those encodings are not emitted by any real toolchain.
		// LLVM's MicrosoftDemangle / MicrosoftMangle and Wine's undname
		// reimplementation none of them recognize `_X` or `_Y` as type
		// codes. Real cointerface is plain `Y<name>@@` (no underscore) at
		// the top-level type switch, grouped with T/U/V; coclass has no
		// dedicated mangling and is emitted as `V<name>@@` (class). Let
		// `_X` / `_Y` fall through to the `default: throw` so malformed
		// input is rejected instead of producing a bogus class type.
		default:
			throw DemangleException();
		}
		break;
	case '$':
		if (reader.PeekMatch("$Q", 2)) // &&
		{
			reader.Consume(2);
			typeClass = PointerTypeClass;
			refType = RValueReferenceType;
			_const = false;
			_volatile = false;
		}
		else if (reader.PeekMatch("$R", 2)) // && volatile
		{
			reader.Consume(2);
			typeClass = PointerTypeClass;
			refType = RValueReferenceType;
			_const = false;
			_volatile = true;
		}
		else if (reader.PeekMatch("$A", 2))
		{
			reader.Consume(2);
			char num = reader.Read();
			if (num >= '6' && num <= '9')
			{
				// For member function types (8/9), skip the class scope marker @@
				if ((num == '8' || num == '9') && reader.Length() >= 2
					&& reader.Peek() == '@' && reader.PeekAt(1) == '@')
					reader.Consume(2);
				return DemangleFunction(NoNameType, num >= '7', varList, NoneFunctionClass, false);
			}
			throw DemangleException();
		}
		else if (reader.PeekMatch("$C", 2))
		{
			reader.Consume(2);
			DemangleModifiers(_const, _volatile, isMember);
			NameList name;
			newType = DemangleVarType(varList, false, name);
			newType.SetConst(_const);
			newType.SetVolatile(_volatile);
			return newType;
		}
		else if (reader.PeekMatch("$T", 2))
		{
			reader.Consume(2);
			auto t = DemangledTypeNode::NamedType(UnknownNamedTypeClass, vector<string>{"std::nullptr"});
			recordTypeBackref(t);
			return t;
		}
		else if (reader.PeekMatch("$B", 2))
		{
			// $$B is a type modifier (managed/const) - strip and parse underlying type
			reader.Consume(2);
			NameList name;
			return DemangleVarType(varList, isReturn, name);
		}
		else if (reader.Peek() == '0')
		{
			reader.Consume();
			int64_t value;
			DemangleNumber(value);
			return DemangledTypeNode::NamedType(UnknownNamedTypeClass, StringList{to_string(value)});
		}
		else if (reader.Peek() == 'D')
		{
			// $D<type> - template type alias / anonymous type parameter
			reader.Consume();
			NameList name;
			return DemangleVarType(varList, isReturn, name);
		}
		else if (reader.Peek() == 'M')
		{
			// $M<type><value> - C++17 `auto` non-type template parameter.
			// The encoded type is the deduced type for the following bare
			// non-type payload and is not itself printed as a template arg.
			reader.Consume();
			NameList autoTypeName;
			DemangleVarType(varList, false, autoTypeName);
			return DemangleAutoNonTypeTemplateParam(varList);
		}
		else if (reader.Peek() == 'H' || reader.Peek() == 'I' || reader.Peek() == 'J')
		{
			// $H/$I/$J - member function pointer value as a non-type template
			// parameter. Format: $H<mangled-symbol><adjustment-number>@;
			// $I has two adjustment numbers, $J has three.
			char kind = reader.Read();
			BackrefList symbolBackrefs = varList;
			auto context = DemangleSymbol(symbolBackrefs);
			_STD_STRING value = "{" + context.type.GetTypeAndName(FinalizeNameList(m_varName));

			// Read adjustment number(s) — NOT $-prefixed, just raw numbers.
			int adjustments = (kind == 'H') ? 1 : (kind == 'I') ? 2 : 3;
			for (int i = 0; i < adjustments && reader.Length() > 0 && reader.Peek() != '@'; i++)
			{
				int64_t adj;
				DemangleNumber(adj);
				value += "," + to_string(adj);
			}
			value += "}";
			return DemangledTypeNode::NamedType(UnknownNamedTypeClass, StringList{value});
		}
		else if (reader.Peek() == '1')
		{
			reader.Consume();
			return DemangleReferencedSymbolValue(varList);
		}
		else
			throw DemangleException();
		break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	{
		MSVC_TRACE("Backref %u %lu", elm - '0', varList.typeList.size());
		auto ref = varList.GetTypeBackrefRef(elm - '0');
		if (outTypeBackref)
			*outTypeBackref = ref;
		return *ref;
	}
	default:
		throw DemangleException();
	}

	switch (typeClass)
	{
	case PointerTypeClass:
	{
		switch (reader.Peek())
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '7':
		case '9':
			throw DemangleException();
		case '6':
		{
			reader.Consume();
			auto childType = DemangleFunction(NoNameType, false, varList, NoneFunctionClass, false);
			newType = DemangledTypeNode::PointerType(std::move(childType),
			                                         _const,
			                                         _volatile,
			                                         refType);
			break;
		}
		case '8': //Named class function pointer
		{
			reader.Consume();
			NameList ownerName;
			DemangleName(ownerName, classFunctionType, varList, true);
			RewriteTemplateBackrefName(ownerName, varList);
			auto childType = DemangleFunction(NoNameType, true, varList, NoneFunctionClass, false);
			newType = DemangledTypeNode::MemberPointerType(std::move(childType),
			                                         std::move(ownerName),
			                                         _const,
			                                         _volatile);
			break;
		}
		default:  // Non-numeric
		{
			MSVC_TRACE("Demangle pointer subtype: '%s'\n", reader.GetRaw());
			DemangledTypeNode child;
			bool _const2 = false, _volatile2 = false, isMember = false;
			NameList ownerName;
			auto suffix = DemanglePointerSuffix();
			ConsumeExtendedModifierPrefix();
			DemangleModifiers(_const2, _volatile2, isMember);
			if (isMember)
			{
				DemangleName(ownerName, classFunctionType, varList, true);
				RewriteTemplateBackrefName(ownerName, varList);
			}
			if (reader.Peek() == 'Y') //Multi-dimentional array
			{
				MSVC_TRACE("Demangle multi-dimentional array");
				int64_t nDimentions;
				reader.Consume();
				DemangleNumber(nDimentions);
				vector<uint64_t> elementList;
				while (nDimentions--)
				{
					int64_t element = 0;
					DemangleNumber(element);
					elementList.push_back(element);
				}
				NameList name;
				child = DemangleVarType(varList, false, name);

				for (auto i = elementList.rbegin(); i != elementList.rend(); i++)
				{
					child = DemangledTypeNode::ArrayType(std::move(child), *i);
				}
			}
			else
			{
				NameList name;
				child = DemangleVarType(varList, true, name, includeImplicitThis && !isMember);
			}

			child.SetConst(_const2);
			child.SetVolatile(_volatile2);
			if (isMember)
			{
				newType = DemangledTypeNode::MemberPointerType(
					std::move(child), std::move(ownerName), _const, _volatile);
			}
			else
			{
				newType = DemangledTypeNode::PointerType(std::move(child),
				                                         _const,
				                                         _volatile,
				                                         refType);
			}

			newType.SetPointerSuffixBits(suffix);
			MSVC_TRACE("Name: %s\n", newType.GetString().c_str());
			break;
		}
		}
		break;
	}
	case EnumerationTypeClass:
		MSVC_TRACE("Demangle enumeration\n");
		DemangleName(typeName, classFunctionType, varList, true);
		newType = DemangledTypeNode::NamedType(EnumNamedTypeClass, typeName, width, _enumSigned);
		break;
	case StructureTypeClass:
		MSVC_TRACE("Demangle structure\n");
		DemangleName(typeName, classFunctionType, varList, true);
		RewriteTemplateBackrefName(typeName, varList);
		switch (structType)
		{
		case ClassStructureType:
			newType = DemangledTypeNode::NamedType(ClassNamedTypeClass, typeName);
			break;
		case StructStructureType:
			newType = DemangledTypeNode::NamedType(StructNamedTypeClass, typeName);
			break;
		case UnionStructureType:
			newType = DemangledTypeNode::NamedType(UnionNamedTypeClass, typeName);
			break;
		default:
			newType = DemangledTypeNode::NamedType(UnknownNamedTypeClass, typeName);
			break;
		}
		break;
	default:
		break;
	}
	if (!isReturn)
	{
		recordTypeBackref(newType);
	}
	return newType;
}


DemangledTypeNode::NodeRef Demangle::TryDemangleVarTypeRef(
	BackrefList& varList, bool isReturn, NameList& name, bool includeImplicitThis)
{
	NestingGuard nestingGuard(*this);
	const char* start = reader.GetRaw();
	size_t typeListSize = varList.typeList.size();
	size_t nameBackrefListSize = varList.nameList.size();
	size_t templateBackrefListSize = varList.templateList.size();
	size_t nameSize = name.size();
	auto restore = [&]() {
		reader.SetRaw(start);
		varList.typeList.resize(typeListSize);
		varList.nameList.resize(nameBackrefListSize);
		varList.templateList.resize(templateBackrefListSize);
		name.resize(nameSize);
	};
	auto makeRef = [&](DemangledTypeNode&& type, bool recordType) -> DemangledTypeNode::NodeRef {
		auto ref = DemangledTypeNode::CreateShared(std::move(type));
		if (recordType && !isReturn)
			varList.PushTypeBackref(ref);
		return ref;
	};

	try
	{
		bool cnst = false, vltl = false;
		BNReferenceType refType = PointerReferenceType;
		BNNameType classFunctionType = NoNameType;
		char elm = reader.Read();
		switch (elm)
		{
		case 'A':
			refType = ReferenceReferenceType;
			break;
		case 'B':
			refType = ReferenceReferenceType;
			vltl = true;
			break;
		case 'C': return makeRef(DemangledTypeNode::IntegerType(1, true, "signed char"), false);
		case 'D': return makeRef(DemangledTypeNode::IntegerType(1, true), false);
		case 'E': return makeRef(DemangledTypeNode::IntegerType(1, false), false);
		case 'F': return makeRef(DemangledTypeNode::IntegerType(2, true), false);
		case 'G': return makeRef(DemangledTypeNode::IntegerType(2, false), false);
		case 'H': return makeRef(DemangledTypeNode::IntegerType(4, true), false);
		case 'I': return makeRef(DemangledTypeNode::IntegerType(4, false), false);
		case 'J': return makeRef(DemangledTypeNode::IntegerType(4, true, "long"), false);
		case 'K': return makeRef(DemangledTypeNode::IntegerType(4, false, "unsigned long"), false);
		case 'M': return makeRef(DemangledTypeNode::FloatType(4), false);
		case 'N': return makeRef(DemangledTypeNode::FloatType(8), false);
		case 'O': return makeRef(DemangledTypeNode::FloatType(10, "long double"), false);
		case 'P':
			break;
		case 'Q':
			cnst = true;
			break;
		case 'R':
			vltl = true;
			break;
		case 'S':
			cnst = true;
			vltl = true;
			break;
		case 'T':
		case 'U':
		case 'V':
		{
			NameList typeName;
			DemangleName(typeName, classFunctionType, varList, true);
			RewriteTemplateBackrefName(typeName, varList);
			switch (elm)
			{
			case 'T': return makeRef(DemangledTypeNode::NamedType(UnionNamedTypeClass, typeName), true);
			case 'U': return makeRef(DemangledTypeNode::NamedType(StructNamedTypeClass, typeName), true);
			case 'V': return makeRef(DemangledTypeNode::NamedType(ClassNamedTypeClass, typeName), true);
			default: break;
			}
			restore();
			return nullptr;
		}
		case 'W':
		{
			size_t width = 0;
			bool enumSigned = false;
			switch (reader.Read())
			{
			case '0': width = 1; enumSigned = true; break;
			case '1': width = 1; enumSigned = false; break;
			case '2': width = 2; enumSigned = true; break;
			case '3': width = 2; enumSigned = false; break;
			case '4': width = 4; enumSigned = true; break;
			case '5': width = 4; enumSigned = false; break;
			case '6': width = 4; enumSigned = true; break;
			case '7': width = 4; enumSigned = false; break;
			default:
				restore();
				return nullptr;
			}
			NameList typeName;
			DemangleName(typeName, classFunctionType, varList, true);
			return makeRef(DemangledTypeNode::NamedType(EnumNamedTypeClass, typeName, width, enumSigned), true);
		}
		case 'X': return makeRef(DemangledTypeNode::VoidType(), false);
		case 'Z': return makeRef(DemangledTypeNode::VarArgsType(), false);
		case '?':
		{
			if (reader.Peek() >= '0' && reader.Peek() <= '9')
			{
				size_t reference = reader.Read() - '0';
				try
				{
					return varList.GetTypeBackrefRef(reference);
				}
				catch (const DemangleException&)
				{
					if (reference == 2)
						return makeRef(DemangledTypeNode::NamedType(UnknownNamedTypeClass, vector<string>{"auto"}), false);
					throw;
				}
			}
			if (reader.Peek() != '<')
			{
				restore();
				return nullptr;
			}

			string placeholder = reader.ReadUntil('@');
			if (reader.Peek() == '@')
				reader.Consume();
			if (placeholder == "<auto>")
				return makeRef(DemangledTypeNode::NamedType(UnknownNamedTypeClass, vector<string>{"auto"}), false);
			if (placeholder == "<decltype-auto>")
				return makeRef(DemangledTypeNode::NamedType(UnknownNamedTypeClass, vector<string>{"decltype(auto)"}), false);
			return makeRef(DemangledTypeNode::NamedType(UnknownNamedTypeClass, vector<string>{placeholder}), false);
		}
		case '_':
			switch (reader.Read())
			{
			case 'D': return makeRef(DemangledTypeNode::IntegerType(1, true), true);
			case 'E': return makeRef(DemangledTypeNode::IntegerType(1, false), true);
			case 'F': return makeRef(DemangledTypeNode::IntegerType(2, true), true);
			case 'G': return makeRef(DemangledTypeNode::IntegerType(2, false), true);
			case 'H': return makeRef(DemangledTypeNode::IntegerType(4, true), true);
			case 'I': return makeRef(DemangledTypeNode::IntegerType(4, false), true);
			case 'J': return makeRef(DemangledTypeNode::IntegerType(8, true), true);
			case 'K': return makeRef(DemangledTypeNode::IntegerType(8, false), true);
			case 'L': return makeRef(DemangledTypeNode::IntegerType(16, true), true);
			case 'M': return makeRef(DemangledTypeNode::IntegerType(16, false), true);
			case 'N': return makeRef(DemangledTypeNode::BoolType(), true);
			case 'S': return makeRef(DemangledTypeNode::WideCharType(2, "char16_t"), true);
			case 'U': return makeRef(DemangledTypeNode::WideCharType(4, "char32_t"), true);
			case 'W': return makeRef(DemangledTypeNode::WideCharType(2, "wchar_t"), true);
			case 'P': return makeRef(DemangledTypeNode::NamedType(UnknownNamedTypeClass, StringList{"auto"}), true);
			case 'Q': return makeRef(DemangledTypeNode::IntegerType(1, true, "char8_t"), true);
			case 'T': return makeRef(DemangledTypeNode::NamedType(UnknownNamedTypeClass, StringList{"decltype(auto)"}), true);
			default:
				restore();
				return nullptr;
			}
		case '$':
			if (reader.PeekMatch("$Q", 2))
			{
				reader.Consume(2);
				refType = RValueReferenceType;
			}
			else if (reader.PeekMatch("$R", 2))
			{
				reader.Consume(2);
				refType = RValueReferenceType;
				vltl = true;
			}
			else if (reader.PeekMatch("$T", 2))
			{
				reader.Consume(2);
				return makeRef(DemangledTypeNode::NamedType(UnknownNamedTypeClass, vector<string>{"std::nullptr"}), true);
			}
			else
			{
				restore();
				return nullptr;
			}
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			return varList.GetTypeBackrefRef(elm - '0');
		default:
			restore();
			return nullptr;
		}

		switch (reader.Peek())
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			restore();
			return nullptr;
		default:
			break;
		}

		bool childConst = false;
		bool childVolatile = false;
		bool isMember = false;
		auto suffix = DemanglePointerSuffix();
		ConsumeExtendedModifierPrefix();
		DemangleModifiers(childConst, childVolatile, isMember);
		if (isMember || reader.Peek() == 'Y')
		{
			restore();
			return nullptr;
		}

		NameList childName;
		auto childRef = TryDemangleVarTypeRef(varList, true, childName, includeImplicitThis);
		if (!childRef || !childName.empty())
		{
			restore();
			return nullptr;
		}

		if (childConst || childVolatile)
		{
			DemangledTypeNode child = *childRef;
			child.SetConst(childConst);
			child.SetVolatile(childVolatile);
			childRef = DemangledTypeNode::CreateShared(std::move(child));
		}

		auto pointerType = DemangledTypeNode::PointerType(childRef, cnst, vltl, refType);
		pointerType.SetPointerSuffixBits(suffix);
		return makeRef(std::move(pointerType), true);
	}
	catch (const DemangleException&)
	{
		restore();
		return nullptr;
	}
}


void Demangle::DemangleNumber(int64_t& num)
{
	MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());
	num = 0;
	int mult = 1;
	if (reader.Peek() == '?')
	{
		mult = -1;
		reader.Consume();
	}

	//The number is decimal 1-10
	if (reader.Peek() >= '0' && reader.Peek() <= '9')
	{
		num = mult * (reader.Read() + 1 - '0');
		return;
	}
	else
	{
		//The number is hexidecimal
		string strnum = reader.ReadUntil('@');
		for (auto a : strnum)
		{
			num *= 16;
			if (a >= 'A' && a <= 'P')
				num += a - 'A';
			else
				throw DemangleException();
		}
		num *= mult;
		return;
	}
}


void Demangle::DemangleChar(char& ch)
{
	MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());
	// Basic char is just the char
	if (reader.Peek() != '?')
	{
		ch = reader.Peek();
		reader.Consume();
		return;
	}
	reader.Consume();

	// Hex char is ?$XX for 2 hex digits XX
	if (reader.Peek() == '$')
	{
		MSVC_TRACE("%s: Hex digit '%s'\n", __FUNCTION__, reader.GetRaw());

		reader.Consume();
		char c1 = reader.Peek();
		reader.Consume();
		char c2 = reader.Peek();
		reader.Consume();

		if (c1 < 'A' || c1 > 'P')
			throw DemangleException("Invalid character");
		if (c2 < 'A' || c2 > 'P')
			throw DemangleException("Invalid character");

		uint8_t b1 = c1 - 'A';
		uint8_t b2 = c2 - 'A';

		ch = (char)((b1 << 4) | b2);
		return;
	}

	MSVC_TRACE("%s: Table lookup '%s'\n", __FUNCTION__, reader.GetRaw());

	// Otherwise it's a lookup based on some big table
	// Thanks, LLVM!
	switch (reader.Peek())
	{
	case '0': ch = ','; reader.Consume(); return;
	case '1': ch = '/'; reader.Consume(); return;
	case '2': ch = '\\'; reader.Consume(); return;
	case '3': ch = ':'; reader.Consume(); return;
	case '4': ch = '.'; reader.Consume(); return;
	case '5': ch = ' '; reader.Consume(); return;
	case '6': ch = '\n'; reader.Consume(); return;
	case '7': ch = '\t'; reader.Consume(); return;
	case '8': ch = '\''; reader.Consume(); return;
	case '9': ch = '-'; reader.Consume(); return;
	case 'a': ch = '\xE1'; reader.Consume(); return;
	case 'b': ch = '\xE2'; reader.Consume(); return;
	case 'c': ch = '\xE3'; reader.Consume(); return;
	case 'd': ch = '\xE4'; reader.Consume(); return;
	case 'e': ch = '\xE5'; reader.Consume(); return;
	case 'f': ch = '\xE6'; reader.Consume(); return;
	case 'g': ch = '\xE7'; reader.Consume(); return;
	case 'h': ch = '\xE8'; reader.Consume(); return;
	case 'i': ch = '\xE9'; reader.Consume(); return;
	case 'j': ch = '\xEA'; reader.Consume(); return;
	case 'k': ch = '\xEB'; reader.Consume(); return;
	case 'l': ch = '\xEC'; reader.Consume(); return;
	case 'm': ch = '\xED'; reader.Consume(); return;
	case 'n': ch = '\xEE'; reader.Consume(); return;
	case 'o': ch = '\xEF'; reader.Consume(); return;
	case 'p': ch = '\xF0'; reader.Consume(); return;
	case 'q': ch = '\xF1'; reader.Consume(); return;
	case 'r': ch = '\xF2'; reader.Consume(); return;
	case 's': ch = '\xF3'; reader.Consume(); return;
	case 't': ch = '\xF4'; reader.Consume(); return;
	case 'u': ch = '\xF5'; reader.Consume(); return;
	case 'v': ch = '\xF6'; reader.Consume(); return;
	case 'w': ch = '\xF7'; reader.Consume(); return;
	case 'x': ch = '\xF8'; reader.Consume(); return;
	case 'y': ch = '\xF9'; reader.Consume(); return;
	case 'z': ch = '\xFA'; reader.Consume(); return;
	case 'A': ch = '\xC1'; reader.Consume(); return;
	case 'B': ch = '\xC2'; reader.Consume(); return;
	case 'C': ch = '\xC3'; reader.Consume(); return;
	case 'D': ch = '\xC4'; reader.Consume(); return;
	case 'E': ch = '\xC5'; reader.Consume(); return;
	case 'F': ch = '\xC6'; reader.Consume(); return;
	case 'G': ch = '\xC7'; reader.Consume(); return;
	case 'H': ch = '\xC8'; reader.Consume(); return;
	case 'I': ch = '\xC9'; reader.Consume(); return;
	case 'J': ch = '\xCA'; reader.Consume(); return;
	case 'K': ch = '\xCB'; reader.Consume(); return;
	case 'L': ch = '\xCC'; reader.Consume(); return;
	case 'M': ch = '\xCD'; reader.Consume(); return;
	case 'N': ch = '\xCE'; reader.Consume(); return;
	case 'O': ch = '\xCF'; reader.Consume(); return;
	case 'P': ch = '\xD0'; reader.Consume(); return;
	case 'Q': ch = '\xD1'; reader.Consume(); return;
	case 'R': ch = '\xD2'; reader.Consume(); return;
	case 'S': ch = '\xD3'; reader.Consume(); return;
	case 'T': ch = '\xD4'; reader.Consume(); return;
	case 'U': ch = '\xD5'; reader.Consume(); return;
	case 'V': ch = '\xD6'; reader.Consume(); return;
	case 'W': ch = '\xD7'; reader.Consume(); return;
	case 'X': ch = '\xD8'; reader.Consume(); return;
	case 'Y': ch = '\xD9'; reader.Consume(); return;
	case 'Z': ch = '\xDA'; reader.Consume(); return;
	default:
		throw DemangleException("Unknown character");
	}
}


void Demangle::DemangleWideChar(uint16_t& wch)
{
	char c1, c2;
	DemangleChar(c1);
	DemangleChar(c2);

	wch = (uint16_t)(((uint16_t)c1 << 8) | (uint16_t)c2);
}


void Demangle::DemangleVariableList(vector<DemangledTypeNode::Param>& paramList, BackrefList& varList, bool typeBackrefs)
{
	MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());
	bool _const = false, _volatile = false, isMember = false;
	uint8_t suffix = 0;
	for (;;)
	{
		bool hasModifiers = false;
		if (reader.Peek() == 'Z')
		{
			if (reader.Length() >= 2 && reader.PeekAt(1) == 'Z')
			{
				paramList.push_back({"", DemangledTypeNode::CreateShared(DemangledTypeNode::VarArgsType())});
				reader.Consume();
				continue;
			}
			break;
		}
		if (reader.Peek() == '@')
		{
			reader.Consume();
			break;
		}
		else if (reader.Length() >= 4 && reader.PeekMatch("$$$V", 4))
		{
			// $$$V = empty expanded type / template-template pack (post-MSVC2015 mangling).
			// See clang/lib/AST/MicrosoftMangle.cpp: for MSVC2015-compat this emits $$V,
			// otherwise $$$V.
			reader.Consume(4);
			continue;
		}
		else if (reader.Length() >= 3 && (reader.PeekMatch("$$V", 3) || reader.PeekMatch("$$Z", 3)))
		{
			// $$V = empty expanded type / template-template pack (MSVC2015-compat mangling).
			// $$Z = separator between two consecutive packs (emitted between non-empty packs,
			//       not as a lone template argument). LLVM's demangler leniently skips it in
			//       any position; we follow suit.
			// NB: $$S is NOT emitted by any known toolchain - only $S (single $) is a real
			//     token, handled below.
			reader.Consume(3);
			continue;
		}
		else if (reader.Length() >= 2 && reader.PeekMatch("$S", 2))
		{
			// $S = empty expanded non-type template pack
			// (e.g. `template<int... Ns>` or `template<auto... Vs>` instantiated with zero args).
			reader.Consume(2);
			continue;
		}
		else if (reader.Peek() == '?')
		{
			reader.Consume();
			suffix = DemanglePointerSuffix();
			ConsumeExtendedModifierPrefix();
			DemangleModifiers(_const, _volatile, isMember);
			hasModifiers = true;
		}

		NameList name;
		MSVC_TRACE("Argument %zu: %s", paramList.size(), reader.GetRaw());
		size_t typeListSizeAtEntry = varList.typeList.size();
		DemangledTypeNode::NodeRef parsedType = TryDemangleVarTypeRef(varList, false, name);
		DemangledTypeNode type;
		if (!parsedType)
			type = DemangleVarType(varList, false, name, true, &parsedType);
		// Template argument lists may use temporary backrefs created while
		// parsing an argument, and later arguments may refer to them. However,
		// the completed top-level argument itself is not added as a later type
		// backref in the same template argument list. DemangleVarType appends
		// that completed type last, so preserve any intermediate entries and
		// drop only the final top-level type.
		if (!typeBackrefs && varList.typeList.size() > typeListSizeAtEntry)
			varList.typeList.pop_back();
		if (hasModifiers)
		{
			if (parsedType)
				type = *parsedType;
			type.SetConst(_const);
			type.SetVolatile(_volatile);
			type.SetPointerSuffixBits(suffix);
		}

		DemangledTypeNode::Param vt;
		if (name.size() == 1)
			vt.name = name[0].GetString();
		else if (name.size() > 1)
			vt.name = JoinNameList(name);
		if (hasModifiers || !parsedType)
			vt.type = DemangledTypeNode::CreateShared(std::move(type));
		else
			vt.type = parsedType;
		paramList.push_back(std::move(vt));
		MSVC_TRACE("Argument %zu: '%s' - '%s'\n", paramList.size() - 1, paramList.back().type->GetString().c_str(), reader.GetRaw());
	}
	if (reader.Peek() == 'Z')
		reader.Consume();
	MSVC_TRACE("%s: done '%s'\n", __FUNCTION__, reader.GetRaw());
}


Demangle::NameType Demangle::GetNameType()
{
	if (reader.Peek() == '?')
	{
		reader.Consume();
		if (reader.Peek()== '?')
		{
			reader.Consume();
			// Check for ??@ (MD5 hashed name) after consuming both ?s
			if (reader.Peek() == '@')
			{
				reader.Consume(); // consume '@'
				return NameString; // ReadUntil('@') will get the hash
			}
			return GetNameType();
		}
		else if (reader.Peek() == '$')
		{
			reader.Consume();
			return NameTemplate;
		}
		else if (reader.Peek() == '0')
		{
			reader.Consume();
			return NameConstructor;
		}
		else if (reader.Peek() == '1')
		{
			reader.Consume();
			return NameDestructor;
		}
		else if (reader.Peek() == 'B')
		{
			reader.Consume();
			return NameReturn;
		}
		else if (reader.Length() >= 3 && reader.Peek() == 'A' && reader.PeekAt(1) == '0' && reader.PeekAt(2) == 'x')
		{
			reader.Consume();
			return NameAnonymousNamespace;
		}
		else if (reader.PeekMatch("_R", 2))
		{
			reader.Consume(2);
			return NameRtti;
		}
		else if (reader.Peek() >= 'a' && reader.Peek() <= 'z')
		{
			// Lowercase after ? indicates a non-standard extension name
			// (e.g., ??null$initializer$ for thread-safe static init guards)
			// All standard MSVC operator codes use uppercase/digits/_
			return NameString;
		}
		else
		{
			return NameLookup;
		}
	}
	else if (reader.Peek() >= '0' && reader.Peek() <= '9')
	{
		return NameBackref;
	}
	return NameString;
}


void Demangle::DemangleNameTypeString(string& out)
{
	out = reader.ReadUntil('@');
}


void Demangle::DemangleNameTypeRtti(BNNameType& classFunctionType,
                                    BackrefList& nameBackrefList,
                                    string& out)
{
	DemangledTypeNode rtti;
	switch (reader.Read())
	{
	case '0':
	{
		bool _const = false, _volatile = false, isMember = false;
		uint8_t suffix = 0;
		if (reader.Peek() == '?')
		{
			reader.Consume();
			suffix = DemanglePointerSuffix();
			ConsumeExtendedModifierPrefix();
			DemangleModifiers(_const, _volatile, isMember);
		}

		NameList name;
		rtti = DemangleVarType(nameBackrefList, false, name);
		rtti.SetConst(_const);
		rtti.SetVolatile(_volatile);
		rtti.SetPointerSuffixBits(suffix);
		out = rtti.GetString() + " `RTTI Type Descriptor'";
		classFunctionType = RttiTypeDescriptor;
		break;
	}
	case '1':
		out = "`RTTI Base Class Descriptor at (";
		for (int i = 0; i < 4; i++)
		{
			int64_t num = 0;
			DemangleNumber(num);
			if (i > 0)
			{
				out += ", ";
			}
			out += to_string(num);
		}
		out += ")'";
		classFunctionType = RttiBaseClassDescriptor;
		break;
	case '2':
		out = "`RTTI Base Class Array'";
		classFunctionType = RttiBaseClassArray;
		break;
	case '3':
		out = "`RTTI Class Hierarchy Descriptor'";
		classFunctionType = RttiClassHierarchyDescriptor;
		break;
	case '4':
		out = "`RTTI Complete Object Locator'";
		classFunctionType = RttiCompleteObjectLocator;
		break;
	default: throw DemangleException();
	}
}


void Demangle::DemangleTypeNameLookup(string& out, BNNameType& functionType)
{
	MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());
	switch (reader.Read())
	{
	case '?': functionType = NoNameType; break;
	case '0': functionType = ConstructorNameType; break;
	case '1': functionType = ConstructorNameType; out = "~"; break; // destructor
	case 'B': functionType = OperatorReturnTypeNameType; out = "operator"; break; // conversion operator
	case '2': functionType = OperatorNewNameType; break;
	case '3': functionType = OperatorDeleteNameType; break;
	case '4': functionType = OperatorAssignNameType; break;
	case '5': functionType = OperatorRightShiftNameType; break;
	case '6': functionType = OperatorLeftShiftNameType; break;
	case '7': functionType = OperatorNotNameType; break;
	case '8': functionType = OperatorEqualNameType; break;
	case '9': functionType = OperatorNotEqualNameType; break;
	case 'A': functionType = OperatorArrayNameType; break;
	case 'C': functionType = OperatorArrowNameType; break;
	case 'D': functionType = OperatorStarNameType; break;
	case 'E': functionType = OperatorIncrementNameType; break;
	case 'F': functionType = OperatorDecrementNameType; break;
	case 'G': functionType = OperatorMinusNameType; break;
	case 'H': functionType = OperatorPlusNameType; break;
	case 'I': functionType = OperatorBitAndNameType; break;
	case 'J': functionType = OperatorArrowStarNameType; break;
	case 'K': functionType = OperatorDivideNameType; break;
	case 'L': functionType = OperatorModulusNameType; break;
	case 'M': functionType = OperatorLessThanNameType; break;
	case 'N': functionType = OperatorLessThanEqualNameType; break;
	case 'O': functionType = OperatorGreaterThanNameType; break;
	case 'P': functionType = OperatorGreaterThanEqualNameType; break;
	case 'Q': functionType = OperatorCommaNameType; break;
	case 'R': functionType = OperatorParenthesesNameType; break;
	case 'S': functionType = OperatorTildeNameType; break;
	case 'T': functionType = OperatorXorNameType; break;
	case 'U': functionType = OperatorBitOrNameType; break;
	case 'V': functionType = OperatorLogicalAndNameType; break;
	case 'W': functionType = OperatorLogicalOrNameType; break;
	case 'X': functionType = OperatorStarEqualNameType; break;
	case 'Y': functionType = OperatorPlusEqualNameType; break;
	case 'Z': functionType = OperatorMinusEqualNameType; break;
	case '_':
	{
		MSVC_TRACE(" %s: '%s'\n", __FUNCTION__, reader.GetRaw());
		switch (reader.Read())
		{
		case '0': functionType = OperatorDivideEqualNameType; break;
		case '1': functionType = OperatorModulusEqualNameType; break;
		case '2': functionType = OperatorRightShiftEqualNameType; break;
		case '3': functionType = OperatorLeftShiftEqualNameType; break;
		case '4': functionType = OperatorAndEqualNameType; break;
		case '5': functionType = OperatorOrEqualNameType; break;
		case '6': functionType = OperatorXorEqualNameType; break;
		case '7': functionType = VFTableNameType; break;
		case '8': functionType = VBTableNameType; break;
		case '9': functionType = VCallNameType; break;
		case 'A': functionType = TypeofNameType; break;
		case 'B': functionType = LocalStaticGuardNameType; break;
		case 'C': functionType = StringNameType; break;
		case 'D': functionType = VBaseDestructorNameType; break;
		case 'E': functionType = VectorDeletingDestructorNameType; break;
		case 'F': functionType = DefaultConstructorClosureNameType; break;
		case 'G': functionType = ScalarDeletingDestructorNameType; break;
		case 'H': functionType = VectorConstructorIteratorNameType; break;
		case 'I': functionType = VectorDestructorIteratorNameType; break;
		case 'J': functionType = VectorVBaseConstructorIteratorNameType; break;
		case 'K': functionType = VirtualDisplacementMapNameType; break;
		case 'L': functionType = EHVectorConstructorIteratorNameType; break;
		case 'M': functionType = EHVectorDestructorIteratorNameType; break;
		case 'N': functionType = EHVectorVBaseConstructorIteratorNameType; break;
		case 'O': functionType = CopyConstructorClosureNameType; break;
		case 'P': functionType = UDTReturningNameType; break;
		case 'S': functionType = LocalVFTableNameType; break;
		case 'T': functionType = LocalVFTableConstructorClosureNameType; break;
		case 'U': functionType = OperatorNewArrayNameType; break;
		case 'V': functionType = OperatorDeleteArrayNameType; break;
		case 'X': functionType = PlacementDeleteClosureNameType; break;
		case 'Y': functionType = PlacementDeleteClosureArrayNameType; break;
		case 'Q': // Fallthrough
		case 'W': // Fallthrough
		case 'Z': functionType = NoNameType; break;
		case '_':
		{
			MSVC_TRACE("  %s: '%s'\n", __FUNCTION__, reader.GetRaw());
			const char extendedNameType = reader.Read();
			switch (extendedNameType)
			{
			case 'A': functionType = ManagedVectorConstructorIteratorNameType; break;
			case 'B': functionType = ManagedVectorDestructorIteratorNameType; break;
			case 'C': functionType = EHVectorCopyConstructorIteratorNameType; break;
			// ??__D is the *copy* variant per LLVM (MicrosoftDemangle.cpp:701).
			// Previously routed to EHVectorVBaseConstructorIteratorNameType
			// (the non-copy enum used by ??_O), which dropped the "copy" word.
			case 'D': functionType = EHVectorVBaseCopyConstructorIteratorNameType; break;
			// ??__E and ??__F are not reached here — they're handled at the
			// top level in DemangleSymbol, matching LLVM's special-intrinsic
			// dispatch. See DemangleDynamicInitFini.
			case 'E': // fall through — unreachable in practice
			case 'F': functionType = (extendedNameType == 'E') ? DynamicInitializerNameType : DynamicAtExitDestructorNameType; break;
			case 'G': functionType = VectorCopyConstructorIteratorNameType; break;
			case 'H': functionType = VectorVBaseCopyConstructorIteratorNameType; break;
			case 'I': functionType = ManagedVectorCopyConstructorIteratorNameType; break;
			case 'J': functionType = LocalStaticThreadGuardNameType; break;
			case 'K':
			{
				// User-defined literal operator: ??__K<suffix>@<scope-chain>
				// LLVM's demangleLiteralOperatorIdentifier consumes a simple
				// string terminated by '@' as the literal suffix and renders it
				// as `operator ""<suffix>`. The outer DemangleName loop then
				// picks up any enclosing scope chain as a normal prefix.
				functionType = UserDefinedLiteralOperatorNameType;
				_STD_STRING suffix = reader.ReadUntil('@');
				if (suffix.empty())
					throw DemangleException("??__K requires a non-empty literal suffix");
				out = "operator \"\"" + suffix;
				break;
			}
			case 'L': functionType = NoNameType; out = "operator co_await"; break;
			case 'M': functionType = NoNameType; out = "operator<=>"; break; // spaceship operator
			default: throw DemangleException("Demangle Lookup Failed"); // fall through
			}
			break;
		}
		default:
			throw DemangleException("Demangle Lookup Failed");
		}
		break;
	}
	default: throw DemangleException("Demangle Lookup Failed");
	}
	if (out.empty())
		out = Type::GetNameTypeString(functionType);
}


DemangledNamePart Demangle::DemangleTemplateInstantiationName(BackrefList& nameBackrefList)
{
	DemangledNamePart out;
	MSVC_TRACE("DemangleTemplateInstantiationName: '%s'\n", reader.GetRaw());
	reader.Consume(2);
	if (reader.Peek() >= '0' && reader.Peek() <= '9')
	{
		out = nameBackrefList.GetNameBackref(reader.Read() - '0');
	}
	else
	{
		string name;
		DemangleNameTypeString(name);
		out = MakeNameSegment(name);
	}
	nameBackrefList.PushNameBackref(out);
	return out;
}


DemangledNamePart Demangle::DemangleTemplateInstantiationNameInLocalContext(BackrefList& nameBackrefList)
{
	DemangledNamePart out;
	vector<DemangledTypeNode::Param> params;
	BNNameType dummyFunctionType = NoNameType;
	NameList dummyNameList;
	bool backrefEligible = true;
	MSVC_TRACE("DemangleTemplateInstantiationNameInLocalContext: '%s'\n", reader.GetRaw());

	{
		BackrefContextSwitch localContext(nameBackrefList);
		reader.Consume(2);
		out = DemangleUnqualifiedSymbolName(dummyNameList, nameBackrefList, dummyFunctionType, backrefEligible);
		if (backrefEligible && dummyFunctionType == NoNameType)
			nameBackrefList.PushNameBackref(out);
		DemangleTemplateParams(params, nameBackrefList, out);
	}

	nameBackrefList.PushTemplateSpecialization(out);
	nameBackrefList.PushNameBackref(out);
	return out;
}


void Demangle::DemangleTemplateParams(vector<DemangledTypeNode::Param>& params, BackrefList& nameBackrefList, DemangledNamePart& out)
{
	NestingGuard nestingGuard(*this);
	params.clear();
	const bool nestedTemplateContext = (m_templateParamDepth > 0);
	struct NameBackrefScopeGuard
	{
		BackrefList& backrefs;
		size_t typeCount;
		size_t nameCount;
		~NameBackrefScopeGuard()
		{
			backrefs.typeList.resize(typeCount);
			backrefs.nameList.resize(nameCount);
		}
	};
	struct TemplateDepthGuard
	{
		size_t& depth;
		TemplateDepthGuard(size_t& depth): depth(depth) { depth++; }
		~TemplateDepthGuard() { depth--; }
	};

	{
		TemplateDepthGuard depthGuard(m_templateParamDepth);
		NameBackrefScopeGuard scopeGuard {
			nameBackrefList,
			nameBackrefList.typeList.size(),
			nameBackrefList.nameList.size()
		};

		DemangleVariableList(params, nameBackrefList, false);
	}

	out.SetTemplateArguments(params);
	nameBackrefList.PushTemplateSpecialization(out);
	if (nestedTemplateContext)
		nameBackrefList.PushNameBackref(out);
}


DemangledNamePart Demangle::DemangleUnqualifiedSymbolName(NameList& nameList, BackrefList& nameBackrefList,
	BNNameType& classFunctionType, bool& backrefEligible)
{
	backrefEligible = true;
	DemangledNamePart out;
	string text;
	if (reader.PeekMatch("?$", 2))
	{
		reader.Consume(2);
		out = DemangleTemplateInstantiationName(nameBackrefList);
		nameList.insert(nameList.begin(), out);
	}
	else if (reader.Peek() == '?')
	{
		reader.Consume();
		text.clear();
		DemangleTypeNameLookup(text, classFunctionType);
		out = MakeNameSegment(text);
		// Lookup-based operator names are not normal identifier components and
		// should not satisfy later scope backrefs such as strong_ordering@0@.
		backrefEligible = false;
	}
	else if (reader.Peek() >= '0' && reader.Peek() <= '9')
	{
		out = nameBackrefList.GetNameBackref(reader.Read() - '0');
	}
	else
	{
		DemangleNameTypeString(text);
		out = MakeNameSegment(text);
	}
	return out;
}


DemangledTypeNode Demangle::DemangleString()
{
	MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());
	// ??_C@_<length><crc32>@<name>
	if (reader.Peek() != '_')
	{
		throw DemangleException("Invalid mangled string name");
	}
	reader.Consume();

	// Wide char flag (1 yes / 0 no)
	bool isWideChar = false;
	switch (reader.Peek())
	{
	case '1':
	case '2': // UTF-16/UTF-32 encoding variants
	case '3':
		isWideChar = true;
		break;
	case '0':
		break;
	default:
		throw DemangleException("Invalid mangled string name");
	}
	reader.Consume();

	// Length is just a number

	int64_t lengthRaw;
	DemangleNumber(lengthRaw);
	if (lengthRaw < 0)
	{
		throw DemangleException("Invalid mangled string name");
	}
	uint64_t length = (uint64_t)lengthRaw;

	MSVC_TRACE("%s: Before CRC32 '%s'\n", __FUNCTION__, reader.GetRaw());

	// CRC32 (ignored)
	while (reader.Peek() != '@')
	{
		// Usually 8 bytes but I've seen it be 7 for some ungodly reason
		reader.Consume();
	}

	reader.Consume();

	bool truncated = false;
	string name = "";
	string literalPrefix;
	DemangledTypeNode type;

	// String bytes
	if (isWideChar)
	{
		MSVC_TRACE("%s: Wide string '%s'\n", __FUNCTION__, reader.GetRaw());
		string utf8name;
		literalPrefix = "L";
		// Track the last wide char so we can detect missing null terminator.
		uint16_t lastWch = 1;
		size_t wcharCount = 0;
		while (reader.Peek() != '@')
		{
			uint16_t wch;
			DemangleWideChar(wch);
			lastWch = wch;
			wcharCount++;

			uint8_t chs[2];
			chs[0] = wch & 0xFF;
			chs[1] = wch >> 8;

			// TODO: This is actually UCS2 but we don't have an easy decoder for that
			utf8name += Unicode::UTF16ToUTF8(&chs[0], 2);
		}
		reader.Consume();

		// MSVC string literals always mangle their trailing null. A payload
		// that doesn't end in a wide null means the original was too long to
		// fit in the mangling and was truncated. Matches LLVM's demangler.
		if (wcharCount == 0 || lastWch != 0)
			truncated = true;

		name = Unicode::ToEscapedString(Unicode::GetBlocksForNames({}), false, utf8name.data(), utf8name.size());
		type = DemangledTypeNode::ArrayType(DemangledTypeNode::WideCharType(2), length / 2);
	}
	else
	{
		MSVC_TRACE("%s: Non-wide string '%s'\n", __FUNCTION__, reader.GetRaw());
		uint64_t numNulls = 0;
		size_t endNulls = 0;
		vector<uint8_t> chars;
		while (reader.Peek() != '@')
		{
			char ch;
			DemangleChar(ch);
			if (ch == 0)
			{
				numNulls++;
				endNulls++;
			}
			else
			{
				endNulls = 0;
			}
			chars.push_back(ch);
		}
		reader.Consume();

		if (length > (uint64_t)chars.size() + 1)
		{
			truncated = true;
		}
		// MSVC includes the trailing '\0' in the mangled payload. If the last
		// byte isn't a null, the original string was truncated to fit the
		// encoding's size limit — LLVM signals this with a `...` suffix.
		if (!chars.empty() && chars.back() != 0)
			truncated = true;

		// Now time to guess encoding
		if (chars.size() % 1 != 0)
		{
			MSVC_TRACE("%s: Looks like UTF8 '%s'\n", __FUNCTION__, reader.GetRaw());
			name = Unicode::ToEscapedString(Unicode::GetBlocksForNames({}), false, chars.data(), chars.size() - endNulls);
			type = DemangledTypeNode::ArrayType(DemangledTypeNode::IntegerType(1, true), length);
		}
		else
		{
			if (chars.size() % 4 == 0 && numNulls > length * 2 / 3)
			{
				MSVC_TRACE("%s: Looks like UTF32 '%s'\n", __FUNCTION__, reader.GetRaw());
				string utf8name;
				for (size_t i = 0; i < chars.size() - endNulls; i += 4)
				{
					utf8name += Unicode::UTF32ToUTF8(chars.data() + i);
				}
				name = Unicode::ToEscapedString(Unicode::GetBlocksForNames({}), false, utf8name.data(), utf8name.size());
				literalPrefix = "U";
				type = DemangledTypeNode::ArrayType(DemangledTypeNode::WideCharType(4), length / 4);
			}
			else if (numNulls > length / 3)
			{
				MSVC_TRACE("%s: Looks like UTF16 '%s'\n", __FUNCTION__, reader.GetRaw());
				string utf8name;
				for (size_t i = 0; i < chars.size() - endNulls; i += 2)
				{
					utf8name += Unicode::UTF16ToUTF8(chars.data() + i, 2);
				}
				name = Unicode::ToEscapedString(Unicode::GetBlocksForNames({}), false, utf8name.data(), utf8name.size());
				literalPrefix = "L";
				type = DemangledTypeNode::ArrayType(DemangledTypeNode::WideCharType(2), length / 2);
			}
			else
			{
				MSVC_TRACE("%s: Looks like UTF8 '%s'\n", __FUNCTION__, reader.GetRaw());

				name = Unicode::ToEscapedString(Unicode::GetBlocksForNames({}), false, chars.data(), chars.size() - endNulls);
				type = DemangledTypeNode::ArrayType(DemangledTypeNode::IntegerType(1, true), length);
			}
		}
	}
	m_varName.clear();
	m_varName.push_back(MakeNameSegment(fmt::bnformat("{}\"{}\"{}", literalPrefix, name, truncated ? "..." : "")));
	return type;
}


DemangledTypeNode Demangle::DemangleTypeInfoName()
{
	if (reader.Read() != '?')
		throw DemangleException("Unknown raw name type");
	bool _const = false;
	bool _volatile = false;
	bool isMember = false;
	DemangleModifiers(_const, _volatile, isMember);

	MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());

	NameList name;
	DemangledTypeNode type = DemangleVarType(m_backrefList, false, name);
	type.SetConst(_const);
	type.SetVolatile(_volatile);

	switch (type.GetClass())
	{
	case NamedTypeReferenceClass:
	{
		// Match LLVM's demangler: a raw type-info name (.?A...) renders as
		// `<rendered-type> `RTTI Type Descriptor Name''`. Bake the type
		// keyword + name into the symbol's qualified name via m_varName,
		// then return a fresh NamedType marked RttiTypeDescriptor so BN's
		// core type formatter skips its own class/struct prefix — this
		// mirrors the treatment of ??_R0 in DemangleNameTypeRtti case '0'.
		string rendered = type.GetString() + " `RTTI Type Descriptor Name'";
		m_varName = { MakeNameSegment(rendered) };
		NameList rttiTypeName = type.GetName();
		if (rttiTypeName.empty())
			for (const auto& segment: type.RenderTypeNameSegments())
				rttiTypeName.push_back(MakeNameSegment(segment));
		DemangledTypeNode newType = DemangledTypeNode::NamedType(StructNamedTypeClass, std::move(rttiTypeName));
		newType.SetNameType(RttiTypeDescriptor);
		return newType;
	}
	default:
		throw DemangleException("Unexpected type of RTTI Type Name");
	}
}


void Demangle::DemangleName(NameList& nameList,
                            BNNameType& classFunctionType,
                            BackrefList& nameBackrefList,
                            bool typeNameContext)
{
	NestingGuard nestingGuard(*this);
	size_t nameListSizeAtEntry = nameList.size();
	bool pendingConstructorTemplateName = false;

	auto finalizeConstructorTemplateName = [&]() {
		if (!pendingConstructorTemplateName)
			return;

		if (nameList.size() <= nameListSizeAtEntry + 1)
			throw DemangleException("Constructor template missing class scope");

		DemangledNamePart& constructorTemplateName = nameList.back();
		if (!constructorTemplateName.HasTemplateArguments())
			throw DemangleException("Invalid constructor template name");

		// `??$?0...@Class@@` is a templated constructor. LLVM models `?0` as a
		// structor identifier and attaches the parsed enclosing class to it after
			// the qualified name is complete; Wine's undname does the same as a string
			// post-process. Keep the parsed template args and only fill in the
			// constructor's base name here:
			// `?0<Args>` becomes `Class<Args>`.
			constructorTemplateName.SetBase(nameList[nameList.size() - 2].GetString() +
				constructorTemplateName.GetBase());
		};

	auto tryDemangleEscapedLookupScopeName = [&]() -> bool
	{
		if (nameList.size() <= nameListSizeAtEntry)
			return false;

		const char* start = reader.GetRaw();
		if (reader.Length() < 4)
			return false;

		char prefix = start[0];
		if (!((prefix >= 'A' && prefix <= 'Z') || (prefix == '_')))
			return false;
		if (start[1] == '@' || start[1] == '?')
			return false;

		const char* limit = start + reader.Length();
		const char* end = nullptr;
		for (const char* cur = start + 1; (cur + 1) < limit; cur++)
		{
			if ((cur[0] == '@') && (cur[1] == '@'))
			{
				end = cur;
				break;
			}
		}
		if (!end)
			return false;

		vector<string> escapedNames;
		const char* componentStart = start;
		while (componentStart < end)
		{
			const char* componentEnd = componentStart;
			while ((componentEnd < end) && (*componentEnd != '@'))
			{
				char ch = *componentEnd;
				if (ch == '?')
					return false;
				if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z')
				    || (ch >= 'a' && ch <= 'z') || (ch == '_') || (ch == '$')))
				{
					return false;
				}
				componentEnd++;
			}
			if (componentEnd == componentStart)
				return false;

			escapedNames.emplace_back(componentStart, componentEnd - componentStart);
			componentStart = componentEnd + 1;
		}

		for (const auto& escapedName: escapedNames)
		{
			DemangledNamePart escaped = MakeNameSegment(escapedName);
			nameList.insert(nameList.begin(), escaped);
			nameBackrefList.PushNameBackref(std::move(escaped));
		}
		reader.SetRaw(end + 2);
		return true;
	};

	auto decodeEncodedNumber = [&](const string& encoded) -> int64_t
	{
		if (encoded.empty())
			throw DemangleException("Empty encoded number");

		size_t offset = 0;
		int mult = 1;
		if (encoded[offset] == '?')
		{
			mult = -1;
			offset++;
		}
		if (offset >= encoded.size())
			throw DemangleException("Truncated encoded number");

		if (encoded[offset] >= '0' && encoded[offset] <= '9')
		{
			if (offset + 1 != encoded.size())
				throw DemangleException("Decimal encoded number has trailing characters");
			return mult * (encoded[offset] + 1 - '0');
		}

		int64_t num = 0;
		for (; offset < encoded.size(); offset++)
		{
			char a = encoded[offset];
			num *= 16;
			if (a >= 'A' && a <= 'P')
				num += a - 'A';
			else
				throw DemangleException("Invalid encoded hex digit");
		}
		return num * mult;
	};

	auto functionTypeHasPointerSuffix = [&](char ft) -> bool
	{
		return ft != 'C' && ft != 'D' && ft != 'K' && ft != 'L'
			&& ft != 'S' && ft != 'T' && ft != 'Y' && ft != 'Z';
	};

	auto formatFunctionScopeSignature = [&](const DemangledTypeNode& type, const NameList& scopeName) -> string
	{
		string out = type.GetTypeAndName(FinalizeNameList(scopeName));
		while (!out.empty() && out.back() == ' ')
			out.pop_back();
		return out;
	};

	auto appendLocalScope = [&](int64_t scopeOrdinal) -> void
	{
		NameList scopeName;
		BNNameType scopeFunctionType = NoNameType;
		DemangleName(scopeName, scopeFunctionType, nameBackrefList, typeNameContext);

		if (reader.Length() == 0)
			throw DemangleException("Missing local scope function encoding");

		char ft = reader.Read();
		if (ft < 'A' || ft > 'Z')
			throw DemangleException("Invalid local scope function encoding");

		DemangledTypeNode scopeType = DemangleFunction(
			scopeFunctionType, functionTypeHasPointerSuffix(ft), nameBackrefList);

		nameList.insert(nameList.begin(), MakeNameSegment("`" + to_string(scopeOrdinal) + "'"));
		nameList.insert(nameList.begin(), MakeNameSegment("`" + formatFunctionScopeSignature(scopeType, scopeName) + "'"));
	};

	DemangledNamePart out;
	string outText;
	BNNameType functionType;
	BNNameType dummyFunctionType;
	vector<DemangledTypeNode::Param> params;
	while(1)
	{
		MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());
		switch (GetNameType())
		{
		case NameString:
			MSVC_TRACE("Demangle String\n");
			DemangleNameTypeString(outText);
			out = MakeNameSegment(outText);
			nameList.insert(nameList.begin(), out);
			MSVC_TRACE("Pushing backref NameString %s", out.GetString().c_str());
			nameBackrefList.PushNameBackref(std::move(out));
			MSVC_TRACE("nameList.front(): %s\n", nameList.front().GetString().c_str());
			MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());
			break;
		case NameLookup:
		{
			MSVC_TRACE("Demangle Lookup\n");
			if (nameList.size() > nameListSizeAtEntry)
			{
				const char* saved = reader.GetRaw();
				try
				{
					int64_t scopeOrdinal;
					DemangleNumber(scopeOrdinal);
					if (reader.Length() >= 2 && reader.Peek() == '?' && reader.PeekAt(1) == '?')
					{
						appendLocalScope(scopeOrdinal);
						break;
					}
				}
				catch (const DemangleException&)
				{
				}
				reader.SetRaw(saved);
			}
			if (tryDemangleEscapedLookupScopeName())
				break;
			outText.clear();
			DemangleTypeNameLookup(outText, functionType);
			out = MakeNameSegment(outText);
			classFunctionType = functionType;
			nameList.insert(nameList.begin(), out);
			// Check if this is a scope specifier. Scope specifiers are ?<char>
			// followed by either @?? or directly ?? (for digit scopes like ?3??func@...)
			// When nameList has prior components, the operator name is actually a scope index
			// Also handle dynamic init/dtor wrapping ??@ (MD5 hash)
			if (reader.Length() >= 4 && reader.PeekMatch("??@", 3))
			{
				reader.Consume(3); // consume ??@
				_STD_STRING hash = reader.ReadUntil('@');
				nameList.insert(nameList.begin(), MakeNameSegment("??@" + hash + "@"));
				// Consume the trailing @ (name terminator) — the ??@hash@ pattern
				// is followed by @@ (end of scoped name) before the function type
				if (reader.Length() > 0 && reader.Peek() == '@')
					reader.Consume();
				break;
			}
			break;
		}
		case NameAnonymousNamespace:
		{
			DemangleNameTypeString(outText); // discard compiler-generated hash
			nameList.insert(nameList.begin(), MakeNameSegment("`anonymous namespace'"));
			break;
		}
		case NameBackref:
			MSVC_TRACE("Demangle Backref");
			out = nameBackrefList.GetNameBackref(reader.Read() - '0');
			MSVC_TRACE("Demangle Backref: %s", out.GetString().c_str());
			nameList.insert(nameList.begin(), out);
			break;
		case NameTemplate:
		{
			MSVC_TRACE("Demangle Template: '%s'\n", reader.GetRaw());
			BNNameType functionType = NoNameType;
			bool backrefEligible = true;
			if (typeNameContext || (m_templateParamDepth > 0) || (nameList.size() > nameListSizeAtEntry))
			{
				const char* saved = reader.GetRaw();
				reader.SetRaw(saved - 2);
				out = DemangleTemplateInstantiationNameInLocalContext(nameBackrefList);
			}
			else
			{
				out = DemangleUnqualifiedSymbolName(nameList, nameBackrefList, functionType, backrefEligible);
				if (backrefEligible && functionType == NoNameType)
				{
					MSVC_TRACE("Pushing backref NameTemplate %s", out.GetString().c_str());
					nameBackrefList.PushNameBackref(out);
				}
				MSVC_TRACE("Demangling Template variables %s\n", reader.GetRaw());
				DemangleTemplateParams(params, nameBackrefList, out);
				if (functionType == ConstructorNameType)
				{
					classFunctionType = ConstructorNameType;
					pendingConstructorTemplateName = true;
				}
			}
			nameList.insert(nameList.begin(), out);
			break;
		}
		case NameConstructor:
		{
			MSVC_TRACE("NameConstructor\n");
			bool isScope = (nameList.size() > nameListSizeAtEntry);
			if (!isScope)
				classFunctionType = ConstructorNameType;
			if (isScope)
			{
				appendLocalScope(1);
				break;
			}
			DemangleName(nameList, dummyFunctionType, nameBackrefList, typeNameContext);
			if (nameList.size() == 0)
				throw DemangleException();
			nameList.push_back(nameList[nameList.size()-1]);
			return;
		}
		case NameDestructor:
		{
			MSVC_TRACE("NameDestructor\n");
			bool isScope = (nameList.size() > nameListSizeAtEntry);
			if (!isScope)
				classFunctionType = ConstructorNameType;
			if (isScope)
			{
				appendLocalScope(2);
				break;
			}
			DemangleName(nameList, dummyFunctionType, nameBackrefList, typeNameContext);
			if (nameList.size() == 0)
				throw DemangleException();
			nameList.push_back(MakeNameSegment("~" + nameList[nameList.size()-1].GetString()));
			return;
		}
		case NameRtti:
			MSVC_TRACE("NameRtti\n");
			DemangleNameTypeRtti(classFunctionType, nameBackrefList, outText);
			out = MakeNameSegment(outText);
			nameList.insert(nameList.begin(), out);
			break;
		case NameReturn:
		{
			MSVC_TRACE("NameReturn\n");
			if (nameList.size() > nameListSizeAtEntry && reader.Length() >= 1)
			{
				const char* saved = reader.GetRaw();
				try
				{
					_STD_STRING scopeSuffix;
					DemangleNameTypeString(scopeSuffix);
					if (reader.Length() >= 2 && reader.Peek() == '?' && reader.PeekAt(1) == '?')
					{
						appendLocalScope(decodeEncodedNumber("B" + scopeSuffix));
						break;
					}
				}
				catch (const DemangleException&)
				{
				}
				reader.SetRaw(saved);
			}
			classFunctionType = OperatorReturnTypeNameType;
			if (reader.PeekMatch("?$", 2))
			{
				if (m_templateParamDepth > 0)
					out = DemangleTemplateInstantiationNameInLocalContext(nameBackrefList);
				else
					out = DemangleTemplateInstantiationName(nameBackrefList);
				DemangleTemplateParams(params, nameBackrefList, out);
			}
			else
			{
				DemangleNameTypeString(outText);
				out = MakeNameSegment(outText);
				nameBackrefList.PushNameBackref(out);
			}
			nameList.insert(nameList.begin(), out);
			break;
		}
		default:
			throw DemangleException();
		}
		if (reader.Peek() == '@')
		{
			reader.Consume();
			finalizeConstructorTemplateName();
			return;
		}
	}
}



BNCallingConventionName Demangle::DemangleCallingConvention()
{
	MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());
	switch (reader.Read())
	{
	case 'A': //Exported function
	case 'B': return CdeclCallingConvention;
	case 'C': //Exported function
	case 'D': return PascalCallingConvention;
	case 'E': //Exported function
	case 'F': return ThisCallCallingConvention;
	case 'G': //Exported function
	case 'H': return STDCallCallingConvention;
	case 'I': //Exported function
	case 'J': return FastcallCallingConvention;
	case 'K': //Exported function
	case 'L': return NoCallingConvention;
	case 'M': //Exported function
	case 'N': return CLRCallCallingConvention;
	case 'O': //Exported function
	case 'P': return EabiCallCallingConvention;
	case 'Q': return VectorCallCallingConvention;
	case 'S': return SwiftCallingConvention;
	case 'W': return SwiftAsyncCallingConvention;
	default:throw DemangleException();
	}
}


void Demangle::ConsumeExtendedModifierPrefix()
{
	while (reader.PeekMatch("$A", 2))
		reader.Consume(2);
}


uint8_t Demangle::DemanglePointerSuffix()
{
	MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());
	uint8_t suffix = 0;
	if (reader.Peek() == '@')
		return suffix;

	char elm = reader.Peek();
	for (int i = 0; i < 5; i++, elm = reader.Peek())
	{
		if (elm == 'E')
			suffix |= (1u << Ptr64Suffix);
		else if (elm == 'F')
			suffix |= (1u << UnalignedSuffix);
		else if (elm == 'G')
			suffix |= (1u << ReferenceSuffix);
		else if (elm == 'H')
			suffix |= (1u << LvalueSuffix);
		else if (elm == 'I')
			suffix |= (1u << RestrictSuffix);
		else
			break;
		reader.Consume(1);
	}
	return suffix;
}

void Demangle::DemangleModifiers(bool& _const, bool& _volatile, bool &isMember)
{
	MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());
	if (reader.Peek() == '@')
		return;

	_const = false;
	_volatile = false;
	isMember = false;
	char elm = reader.Read();
	switch (elm)
	{
	case 'A': break;
	case 'B': _const = true; break;
	case 'J': _const = true; break;
	case 'C': _volatile = true; break;
	case 'G': _volatile = true; break;
	case 'K': _volatile = true; break;
	case 'D': _const = true; _volatile = true; break;
	case 'H': _const = true; _volatile = true; break;
	case 'L': _const = true; _volatile = true; break;
	case '6': break;
	case '7': break;
	case 'M': break;
	case 'N': break;
	case 'O': _volatile = true; break;
	case 'P': _volatile = true; _const = true; break;
	case 'Q': isMember = true; break;
	case 'U': break;
	case 'Y': break;
	case 'R': _const = true; isMember = true; break;
	case 'V': _const = true; break;
	case 'Z': _const = true; break;
	case 'S': _volatile = true; isMember = true; break;
	case 'W': _volatile = true; break;
	case '0': _volatile = true; break;
	case 'T': _const = true; _volatile = true; isMember = true; break;
	case 'X': _const = true; _volatile = true; break;
	case '1': _const = true; _volatile = true; break;
	case '8': break;
	case '9': break;
	case '2': break;
	case '3': _const = true; break;
	case '4': _volatile = true; break;
	case '5': _const = true; _volatile = true; break;
	case '_':
		elm = reader.Read();
		if (elm == 'A' || elm == 'B')
		{
			//For unhandled "member" and "based" parameters
			break;
		}
		else if (elm == 'C' || elm == 'D')
		{
			//For unhandled "member" and "based" parameters
			break;
		}
		else
		{
			throw DemangleException();
		}
		break;
	default: throw DemangleException();
	}
	return;
}


DemangledTypeNode Demangle::DemangleFunction(BNNameType classFunctionType, bool pointerSuffix, BackrefList& nameBackrefList,
	int funcClass, bool includeImplicitThis)
{
	NestingGuard nestingGuard(*this);
	MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());
	bool _const = false, _volatile = false, isMember = false;
	uint8_t suffix = 0;
	DemangledTypeNode returnType;
	BNCallingConventionName cc;

	//Demangle adjustor which we don't do anything with for now
	if ((funcClass & StaticThunkFunctionClass) == StaticThunkFunctionClass)
	{
		int64_t adjustor;
		DemangleNumber(adjustor);
		AppendToLastNameSegment(m_varName, "`adjustor{" + to_string(adjustor) + "}'");
	}
	else if ((funcClass & VirtualThunkFunctionClass) == VirtualThunkFunctionClass)
	{
		if ((funcClass & VirtualThunkExFunctionClass) == VirtualThunkExFunctionClass)
		{
			int64_t vbptrOffset;
			int64_t vbOffsetOffset;
			int64_t vtorDispOffset;
			int64_t staticOffset;
			DemangleNumber(vbptrOffset);
			DemangleNumber(vbOffsetOffset);
			DemangleNumber(vtorDispOffset);
			DemangleNumber(staticOffset);
			vbptrOffset = SignExtendInt32(vbptrOffset);
			vbOffsetOffset = SignExtendInt32(vbOffsetOffset);
			vtorDispOffset = SignExtendInt32(vtorDispOffset);
			AppendToLastNameSegment(m_varName, "`vtordispex{" + to_string(vbptrOffset) + ", " +
				to_string(vbOffsetOffset) + ", " + to_string(vtorDispOffset) + ", " + to_string(staticOffset) + "}'");
		}
		else
		{
			int64_t vtorDispOffset;
			int64_t staticOffset;
			DemangleNumber(vtorDispOffset);
			DemangleNumber(staticOffset);
			vtorDispOffset = SignExtendInt32(vtorDispOffset);
			AppendToLastNameSegment(m_varName, "`vtordisp{" + to_string(vtorDispOffset) + ", " +
				to_string(staticOffset) + "}'");
		}
	}

	if (pointerSuffix)
	{
		suffix = DemanglePointerSuffix();
		ConsumeExtendedModifierPrefix();
		DemangleModifiers(_const, _volatile, isMember);
	}
	if (reader.Peek() == '?')
		reader.Consume();
	cc = DemangleCallingConvention();
	bool shouldHaveReturnType = true;
	if (reader.Peek() == '@')
	{
		//No return type
		shouldHaveReturnType = false;
		reader.Consume();
		MSVC_TRACE("Function has no return type %s", reader.GetRaw());
	}
	else
	{
		//Demangle function return type
		bool return_const = false, return_volatile = false, isMember = false;
		uint8_t return_suffix = 0;
		bool hasModifiers = false;
		//Check for modifiers before return type
		if (reader.Peek() == '?')
		{
			reader.Consume(1);
			return_suffix = DemanglePointerSuffix();
			DemangleModifiers(return_const, return_volatile, isMember);
			hasModifiers = true;
		}

		NameList name;
		MSVC_TRACE("Demangle function return type %s", reader.GetRaw());
		returnType = DemangleVarType(nameBackrefList, true, name);
		MSVC_TRACE("Return type: %s", returnType.GetString().c_str());
		// '...' (varargs) is only legal as the trailing parameter marker,
		// never as a return type. Reject so we don't build a bogus type.
		if (returnType.GetClass() == VarArgsTypeClass)
			throw DemangleException("Varargs ('Z') is not a valid function return type");
		if (hasModifiers)
		{
			returnType.SetConst(return_const);
			returnType.SetVolatile(return_volatile);
			returnType.SetPointerSuffixBits(return_suffix);
		}
	}
	if (reader.Peek() == '@')
		reader.Consume();

	MSVC_TRACE("\tDemangle Function Parameters %s", reader.GetRaw());
	vector<DemangledTypeNode::Param> params;
	bool needsThisPtr = includeImplicitThis
		&& funcClass != NoneFunctionClass
		&& (funcClass & StaticFunctionClass) != StaticFunctionClass
		&& (funcClass & GlobalFunctionClass) != GlobalFunctionClass;

	DemangleVariableList(params, nameBackrefList);

	if (params.size() >= 1 && params.back().type && params.back().type->GetClass() == VoidTypeClass)
		params.pop_back();

	if (!shouldHaveReturnType)
		returnType = DemangledTypeNode::VoidType();
	DemangledTypeNode newType = DemangledTypeNode::FunctionType(std::move(returnType), nullptr, std::move(params));
	newType.SetConst(_const);
	newType.SetVolatile(_volatile);
	newType.SetPointerSuffixBits(suffix);
	newType.SetNameType(classFunctionType);
	newType.SetCallingConventionName(cc);
	if (needsThisPtr)
	{
		NameList thisName = m_varName;
		if (classFunctionType != OperatorReturnTypeNameType && !thisName.empty())
			thisName.pop_back();
		auto thisNamedType = DemangledTypeNode::NamedType(TypedefNamedTypeClass, std::move(thisName));
		newType.SetImplicitThisParameter(DemangledTypeNode::PointerType(
			std::move(thisNamedType), false, false, PointerReferenceType));
	}

	MSVC_TRACE("Successfully Created Function Type!\n");
	return newType;
}


DemangledTypeNode Demangle::DemangleData(BackrefList& varList)
{
	MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());
	bool _const = false, _volatile = false, isMember = false;
	NameList name;
	DemangledTypeNode newType = DemangleVarType(varList, false, name);
	auto suffix = DemanglePointerSuffix();
	DemangleModifiers(_const, _volatile, isMember);
	if (newType.GetClass() != PointerTypeClass)
	{
		newType.SetConst(_const);
		newType.SetVolatile(_volatile);
		newType.SetPointerSuffixBits(suffix);
	}
	return newType;
}


DemangledTypeNode Demangle::DemanagleRTTI(BNNameType nameType)
{
	MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());
	bool _const = false, _volatile = false, isMember = false;
	if (reader.Length() > 0)
		DemangleModifiers(_const, _volatile, isMember);
	NameList typeName = m_varName;
	MSVC_TRACE("new struct type\n");
	DemangledTypeNode newType = DemangledTypeNode::NamedType(StructNamedTypeClass, typeName);
	newType.SetNameType(nameType);
	newType.SetConst(_const);
	newType.SetVolatile(_volatile);
	MSVC_TRACE("log: %s\n", newType.GetString().c_str());
	return newType;
}


DemangledTypeNode Demangle::DemangleVTable(BackrefList& nameBackrefList)
{
	MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());
	bool _const = false, _volatile = false, isMember = false;
	DemangleModifiers(_const, _volatile, isMember);
	DemangledTypeNode newType = DemangledTypeNode::NamedType(StructNamedTypeClass, m_varName);
	if (reader.Peek() != '@')
	{
		NameList typeName;
		BNNameType classFunctionType = NoNameType;
		DemangleName(typeName, classFunctionType, nameBackrefList, true);
		DemangledNamePart suffix = m_varName.back();
		AppendToLastNameSegment(m_varName, "{for `" + JoinNameList(typeName) + "'}");

		typeName.push_back(suffix);
		newType = DemangledTypeNode::NamedType(StructNamedTypeClass, typeName);
	}
	newType.SetConst(_const);
	newType.SetVolatile(_volatile);
	newType.SetNameType(VFTableNameType);
	return newType;
}


// ??__E (dynamic initializer) / ??__F (dynamic atexit destructor).
//
// LLVM dispatches these at the top level via demangleSpecialIntrinsic -->
// demangleInitFiniStub. The mangling wraps another symbol (either a variable
// or a function) and emits a new function stub that initializes/destroys it:
//
//   ??__E<fn-symbol>                   function form, e.g. ??__Efoo@@YAXXZ
//   ??__E?<var-symbol>@@<fn-encoding>  variable form, e.g. ??__E?foo@@3HA@@YAXXZ
//
// LLVM's output places the descriptor (`dynamic initializer for '<target>'`)
// at file scope — not as a member of the target's enclosing class — and
// interpolates the target name inside backticks/quotes. For the variable
// form, it additionally renders the variable's type inside the inner
// backtick pair: `dynamic initializer for `int foo''.
Demangle::DemangleContext Demangle::DemangleDynamicInitFini(bool isDtor, BackrefList& backrefList)
{
	MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());

	// /d2FH4 may replace a long wrapped target with an MD5 name (??@<hash>@).
	// Parse it before the optional '?' marker below; otherwise the first '?'
	// of the hash spelling is mistaken for IsKnownStaticDataMember.
	NameList innerNameList;
	BNNameType innerClassFunctionType = NoNameType;
	bool isMD5Name = false;
	if (reader.Length() >= 3 && reader.PeekMatch("??@", 3))
	{
		reader.Consume(3);
		_STD_STRING hash = reader.ReadUntil('@');
		innerNameList.push_back(MakeNameSegment("??@" + hash + "@"));
		isMD5Name = true;
	}

	// Optional leading '?' flags the "known static data member" form. LLVM
	// calls this IsKnownStaticDataMember — when present, the mangling is
	// required to carry two trailing '@' before the outer function encoding
	// rather than one.
	bool isKnownStaticDataMember = false;
	if (!isMD5Name && reader.Length() > 0 && reader.Peek() == '?')
	{
		reader.Consume();
		isKnownStaticDataMember = true;
	}

	// Parse the inner symbol's qualified name exactly as any other symbol
	// would. DemangleName handles locally-scoped pieces, anonymous namespaces,
	// templates, etc. so a target like
	//   instance@?1??Get@Globals@@SAAEAU1@XZ@
	// resolves correctly.
	if (!isMD5Name)
		DemangleName(innerNameList, innerClassFunctionType, backrefList);

	const char* prefix = isDtor
		? "`dynamic atexit destructor for "
		: "`dynamic initializer for ";
	BNNameType classFunctionType = isDtor
		? DynamicAtExitDestructorNameType
		: DynamicInitializerNameType;

	_STD_STRING descriptor;

	if (reader.Length() == 0)
		throw DemangleException("Truncated ??__E/??__F");

	char next = reader.Peek();
	if (next >= '0' && next <= '4')
	{
		// Variable form: <storage-class><type-encoding> <@-terminators>
		// <outer-function-encoding>. We don't attach the storage class to
		// anything — it exists only to disambiguate variable-vs-function
		// inside the wrapper and to match the mangling grammar.
		reader.Consume(); // storage class
		DemangledTypeNode varType = DemangleData(backrefList);
		_STD_STRING varTypeStr = varType.GetString();
		_STD_STRING innerJoined = JoinNameList(innerNameList);
		descriptor = _STD_STRING(prefix) + "`" + varTypeStr + " " + innerJoined + "''";

		// Consume the @-terminators between the inner variable encoding and
		// the outer function encoding. LLVM requires two when the optional
		// leading '?' was present, one otherwise.
		int atCount = isKnownStaticDataMember ? 2 : 1;
		for (int i = 0; i < atCount; i++)
		{
			if (reader.Length() == 0 || reader.Read() != '@')
				throw DemangleException("Expected '@' terminator in ??__E/??__F variable form");
		}
	}
	else
	{
		// Function form: the inner symbol's function encoding follows
		// directly. The outer stub reuses that encoding (there's no separate
		// outer signature).
		if (isKnownStaticDataMember)
			throw DemangleException("??__E/??__F with leading '?' but no variable form");
		if (isMD5Name)
		{
			while (reader.Length() > 0 && reader.Peek() == '@')
				reader.Consume();
		}
		_STD_STRING innerJoined = JoinNameList(innerNameList);
		descriptor = _STD_STRING(prefix) + "'" + innerJoined + "''";
	}

	// Replace the symbol's qualified name with just the descriptor — this is
	// what puts the output at file scope with no enclosing class prefix.
	m_varName = { MakeNameSegment(descriptor) };

	// Parse the outer function encoding. MSVC emits a global cdecl stub
	// ('Y'/'Z') in practice but we dispatch through the full table for
	// robustness (private/public/static/etc.).
	if (reader.Length() == 0)
		throw DemangleException("Truncated ??__E/??__F outer function encoding");
	char funcType = reader.Read();
	switch (funcType)
	{
	case 'A': return { DemangleFunction(classFunctionType, true,  backrefList, PrivateFunctionClass),                              PrivateAccess,   NoScope     };
	case 'B': return { DemangleFunction(classFunctionType, true,  backrefList, PrivateFunctionClass),                              PrivateAccess,   NoScope     };
	case 'C': return { DemangleFunction(classFunctionType, false, backrefList, PrivateFunctionClass | StaticFunctionClass),        PrivateAccess,   StaticScope };
	case 'D': return { DemangleFunction(classFunctionType, false, backrefList, PrivateFunctionClass | StaticFunctionClass),        PrivateAccess,   StaticScope };
	case 'I': return { DemangleFunction(classFunctionType, true,  backrefList, ProtectedFunctionClass),                            ProtectedAccess, NoScope     };
	case 'J': return { DemangleFunction(classFunctionType, true,  backrefList, ProtectedFunctionClass),                            ProtectedAccess, NoScope     };
	case 'K': return { DemangleFunction(classFunctionType, false, backrefList, ProtectedFunctionClass | StaticFunctionClass),      ProtectedAccess, StaticScope };
	case 'L': return { DemangleFunction(classFunctionType, false, backrefList, ProtectedFunctionClass | StaticFunctionClass),      ProtectedAccess, StaticScope };
	case 'Q': return { DemangleFunction(classFunctionType, true,  backrefList, PublicFunctionClass),                               PublicAccess,    NoScope     };
	case 'R': return { DemangleFunction(classFunctionType, true,  backrefList, PublicFunctionClass),                               PublicAccess,    NoScope     };
	case 'S': return { DemangleFunction(classFunctionType, false, backrefList, PublicFunctionClass | StaticFunctionClass),         PublicAccess,    StaticScope };
	case 'T': return { DemangleFunction(classFunctionType, false, backrefList, PublicFunctionClass | StaticFunctionClass),         PublicAccess,    StaticScope };
	case 'Y': return { DemangleFunction(classFunctionType, false, backrefList, GlobalFunctionClass),                               NoAccess,        NoScope     };
	case 'Z': return { DemangleFunction(classFunctionType, false, backrefList, GlobalFunctionClass),                               NoAccess,        NoScope     };
	default:
		throw DemangleException(_STD_STRING("Unexpected outer function type '") + funcType + "' in ??__E/??__F");
	}
}


Demangle::DemangleContext Demangle::DemangleSymbol()
{
	return DemangleSymbol(m_backrefList);
}


Demangle::DemangleContext Demangle::DemangleSymbol(BackrefList& backrefList)
{
	NestingGuard nestingGuard(*this);
	MSVC_TRACE("%s: '%s'\n", __FUNCTION__, reader.GetRaw());
	BNNameType classFunctionType = NoNameType;
	NameList varName;

	if (reader.Peek() == '.')
	{
		reader.Consume();

		return { DemangleTypeInfoName(), NoAccess, NoScope };
	}

	if (reader.Read() != '?')
	{
		throw DemangleException();
	}

	// MD5-hashed names: ??@<32hex>@
	if (reader.Length() >= 2 && reader.PeekMatch("?@", 2))
	{
		reader.Consume(2); // consume ?@
		_STD_STRING hash = reader.ReadUntil('@');
		m_varName.push_back(MakeNameSegment("??@" + hash + "@"));
		return { DemangledTypeNode::VoidType(), NoAccess, NoScope };
	}

	// Special intrinsics dispatched at the top level (matches LLVM's
	// demangleSpecialIntrinsic). ??__E/??__F have a non-uniform grammar
	// that the normal DemangleName scope-chain loop can't express — the
	// bytes after the code are a wrapped inner symbol, not scope prefixes.
	if (reader.Length() >= 4 && (reader.PeekMatch("?__E", 4) || reader.PeekMatch("?__F", 4)))
	{
		bool isDtor = reader.PeekAt(3) == 'F';
		reader.Consume(4); // consume ?__E or ?__F
		return DemangleDynamicInitFini(isDtor, backrefList);
	}

	DemangleName(varName, classFunctionType, backrefList);
	MSVC_TRACE("Done demangling Name: '%s' - '%s'", JoinNameList(varName).c_str(), reader.GetRaw());
	m_varName = varName;

	DemangleContext context;

	if (classFunctionType == StringNameType)
	{
		context = { DemangleString(), NoAccess, NoScope };
		return context;
	}

	// ??__J (local static thread guard) is a *variable*, not a function, per
	// LLVM's demangleLocalStaticGuard. The only valid storage-class suffixes
	// are '4' (IsVisible=false, followed by type encoding like 'IA' for int)
	// and '5' (IsVisible=true). Any other byte here indicates a malformed
	// symbol — e.g. the function-form ??__JFoo@@YAXXZ that earlier permissive
	// code would accept and misrender.
	if (classFunctionType == LocalStaticThreadGuardNameType)
	{
		if (reader.Length() == 0)
			throw DemangleException("Truncated ??__J");
		char next = reader.Peek();
		if (next != '4' && next != '5')
			throw DemangleException("??__J requires variable storage class ('4' or '5'), got '" + _STD_STRING(1, next) + "'");
	}

	char funcType = reader.Read();
	switch(funcType)
	{
	case '0': context = {DemangleData(backrefList),                      PrivateAccess,   StaticScope }; break;
	case '1': context = {DemangleData(backrefList),                      ProtectedAccess, StaticScope }; break;
	case '2': context = {DemangleData(backrefList),                      PublicAccess,    StaticScope }; break;
	case '3': context = {DemangleData(backrefList),                      NoAccess,        NoScope     }; break;
	case '4': context = {DemangleData(backrefList),                      NoAccess,        NoScope     }; break;
	case '5': context = {DemangleVTable(backrefList),                    NoAccess,        NoScope     }; break;
	case '6': context = {DemangleVTable(backrefList),                    NoAccess,        NoScope     }; break;
	case '7': context = {DemangleVTable(backrefList),                    NoAccess,        NoScope     }; break;
	case '8': context = {DemanagleRTTI(classFunctionType),    NoAccess,        NoScope     }; break;
	case '9': context = {DemanagleRTTI(classFunctionType),    NoAccess,        NoScope     }; break;
	case 'A': context = {DemangleFunction(classFunctionType, true,  backrefList, PrivateFunctionClass),                              PrivateAccess,   NoScope     }; break;
	case 'B': context = {DemangleFunction(classFunctionType, true,  backrefList, PrivateFunctionClass),                              PrivateAccess,   NoScope     }; break;
	case 'C': context = {DemangleFunction(classFunctionType, false, backrefList, PrivateFunctionClass | StaticFunctionClass),        PrivateAccess,   StaticScope }; break;
	case 'D': context = {DemangleFunction(classFunctionType, false, backrefList, PrivateFunctionClass | StaticFunctionClass),        PrivateAccess,   StaticScope }; break;
	case 'E': context = {DemangleFunction(classFunctionType, true,  backrefList, PrivateFunctionClass | VirtualFunctionClass),       PrivateAccess,   VirtualScope}; break;
	case 'F': context = {DemangleFunction(classFunctionType, true,  backrefList, PrivateFunctionClass | VirtualFunctionClass),       PrivateAccess,   VirtualScope}; break;
	case 'G': context = {DemangleFunction(classFunctionType, true,  backrefList, PrivateFunctionClass | StaticThunkFunctionClass),   PrivateAccess,   ThunkScope  }; break;
	case 'H': context = {DemangleFunction(classFunctionType, true,  backrefList, PrivateFunctionClass | StaticThunkFunctionClass),   PrivateAccess,   ThunkScope  }; break;
	case 'I': context = {DemangleFunction(classFunctionType, true,  backrefList, ProtectedFunctionClass),                            ProtectedAccess, NoScope     }; break;
	case 'J': context = {DemangleFunction(classFunctionType, true,  backrefList, ProtectedFunctionClass),                            ProtectedAccess, NoScope     }; break;
	case 'K': context = {DemangleFunction(classFunctionType, false, backrefList, ProtectedFunctionClass | StaticFunctionClass),      ProtectedAccess, StaticScope }; break;
	case 'L': context = {DemangleFunction(classFunctionType, false, backrefList, ProtectedFunctionClass | StaticFunctionClass),      ProtectedAccess, StaticScope }; break;
	case 'M': context = {DemangleFunction(classFunctionType, true,  backrefList, ProtectedFunctionClass | VirtualFunctionClass),     ProtectedAccess, VirtualScope}; break;
	case 'N': context = {DemangleFunction(classFunctionType, true,  backrefList, ProtectedFunctionClass | VirtualFunctionClass),     ProtectedAccess, VirtualScope}; break;
	case 'O': context = {DemangleFunction(classFunctionType, true,  backrefList, ProtectedFunctionClass | StaticThunkFunctionClass), ProtectedAccess, ThunkScope  }; break;
	case 'P': context = {DemangleFunction(classFunctionType, true,  backrefList, ProtectedFunctionClass | StaticThunkFunctionClass), ProtectedAccess, ThunkScope  }; break;
	case 'Q': context = {DemangleFunction(classFunctionType, true,  backrefList, PublicFunctionClass),                               PublicAccess,    NoScope     }; break;
	case 'R': context = {DemangleFunction(classFunctionType, true,  backrefList, PublicFunctionClass),                               PublicAccess,    NoScope     }; break;
	case 'S': context = {DemangleFunction(classFunctionType, false, backrefList, PublicFunctionClass | StaticFunctionClass),         PublicAccess,    StaticScope }; break;
	case 'T': context = {DemangleFunction(classFunctionType, false, backrefList, PublicFunctionClass | StaticFunctionClass),         PublicAccess,    StaticScope }; break;
	case 'U': context = {DemangleFunction(classFunctionType, true,  backrefList, PublicFunctionClass | VirtualFunctionClass),        PublicAccess,    VirtualScope}; break;
	case 'V': context = {DemangleFunction(classFunctionType, true,  backrefList, PublicFunctionClass | VirtualFunctionClass),        PublicAccess,    VirtualScope}; break;
	case 'W': context = {DemangleFunction(classFunctionType, true,  backrefList, PublicFunctionClass | StaticThunkFunctionClass),    PublicAccess,    ThunkScope  }; break;
	case 'X': context = {DemangleFunction(classFunctionType, true,  backrefList, PublicFunctionClass | StaticThunkFunctionClass),    PublicAccess,    ThunkScope  }; break;
	case 'Y': context = {DemangleFunction(classFunctionType, false, backrefList, GlobalFunctionClass),                               NoAccess,        NoScope     }; break;
	case 'Z': context = {DemangleFunction(classFunctionType, false, backrefList, GlobalFunctionClass),                               NoAccess,        NoScope     }; break;
	case '$':
	{
		if (reader.Peek() == 'B')
		{
			// Vcall thunk: $B<encoded_offset><calling_convention><this_type>
			reader.Consume();
			int64_t offset;
			DemangleNumber(offset);
			m_varName.back() = MakeNameSegment("`vcall'{" + to_string(offset) + ", {flat}}'");
			// Consume calling convention char + this-type flag char
			if (reader.Length() >= 1)
				reader.Consume(); // calling convention (A=cdecl, etc.)
			if (reader.Length() >= 1 && reader.Peek() != '@')
				reader.Consume(); // this-type flag
			context = {DemangledTypeNode::VoidType(), NoAccess, NoScope};
			break;
		}
		int funcClass = VirtualThunkFunctionClass;
		if (reader.Peek() == 'R')
		{
			reader.Consume();
			funcClass |= VirtualThunkExFunctionClass;
		}
		char thunkType = reader.Read();
		switch (thunkType)
		{
		case '0': context = {DemangleFunction(classFunctionType, true, backrefList, funcClass | VirtualFunctionClass | PrivateFunctionClass),   PrivateAccess,   ThunkScope}; break;
		case '1': context = {DemangleFunction(classFunctionType, true, backrefList, funcClass | VirtualFunctionClass | PrivateFunctionClass),   PrivateAccess,   ThunkScope}; break;
		case '2': context = {DemangleFunction(classFunctionType, true, backrefList, funcClass | VirtualFunctionClass | ProtectedFunctionClass), ProtectedAccess, ThunkScope}; break;
		case '3': context = {DemangleFunction(classFunctionType, true, backrefList, funcClass | VirtualFunctionClass | ProtectedFunctionClass), ProtectedAccess, ThunkScope}; break;
		case '4': context = {DemangleFunction(classFunctionType, true, backrefList, funcClass | VirtualFunctionClass | PublicFunctionClass),    PublicAccess,    ThunkScope}; break;
		case '5': context = {DemangleFunction(classFunctionType, true, backrefList, funcClass | VirtualFunctionClass | PublicFunctionClass),    PublicAccess,    ThunkScope}; break;
		default: throw DemangleException("Unknown virtual thunk type " + string(1, thunkType));
		}
		break;
	}
	default:  throw DemangleException("Unknown function type " + string(1, funcType));
	}
	return context;
}

std::pair<Ref<Type>, QualifiedName> Demangle::Finalize(BinaryView* view)
{
	DemangleContext context = DemangleSymbol();

	Ref<Platform> platformRef = m_platform;
	Platform* platform = platformRef.GetPtr();
	Ref<Platform> viewPlatform;
	if (!platform && view)
	{
		viewPlatform = view->GetDefaultPlatform();
		platform = viewPlatform.GetPtr();
	}

	Architecture* arch = m_arch;
#ifdef BINARYNINJACORE_LIBRARY
	if (!arch && platform)
		arch = platform->GetArchitecture();
	if (!arch && view)
		arch = view->GetDefaultArchitecture();
#else
	Ref<Architecture> viewArch;
	Ref<Architecture> platformArch;
	if (!arch && platform)
	{
		platformArch = platform->GetArchitecture();
		arch = platformArch.GetPtr();
	}
	if (!arch && view)
	{
		viewArch = view->GetDefaultArchitecture();
		arch = viewArch.GetPtr();
	}
#endif
	if (!arch)
		throw DemangleException();

	Ref<Platform> archPlatform;
	if (!platform)
	{
		archPlatform = arch->GetStandalonePlatform();
		platform = archPlatform.GetPtr();
	}

	return {context.type.Finalize(platform), QualifiedName(FinalizeNameList(m_varName))};
}

std::pair<Ref<Type>, QualifiedName> Demangle::Finalize()
{
	return Finalize(m_view.GetPtr());
}

bool Demangle::DemangleMS(Architecture* arch, const string& mangledName, Ref<Type>& outType,
                          QualifiedName& outVarName, const Ref<BinaryView>& view)
{
	outType = nullptr;
	if (mangledName.empty() || (mangledName[0] != '?' && mangledName[0] != '.'))
		return false;
	try
	{
		if (view)
		{
			Demangle demangle(arch, mangledName);
			auto result = demangle.Finalize(view.GetPtr());
			outType = std::move(result.first);
			outVarName = std::move(result.second);
			return true;
		}
		return DemangleMS(arch, mangledName, outType, outVarName);
	}
	catch (DemangleException &e)
	{
		LogDebugForException(e, "Demangling Failed '%s' '%s;", mangledName.c_str(), e.what());
		return false;
	}
}

bool Demangle::DemangleMS(Architecture* arch, const string& mangledName, Ref<Type>& outType,
                          QualifiedName& outVarName, BinaryView* view)
{
	outType = nullptr;
	if (mangledName.empty() || (mangledName[0] != '?' && mangledName[0] != '.'))
		return false;
	if (view)
		return DemangleMS(arch, mangledName, outType, outVarName, Ref<BinaryView>(view));
	return DemangleMS(arch, mangledName, outType, outVarName);
}

bool Demangle::DemangleMS(Platform* platform, const string& mangledName, Ref<Type>& outType,
                          QualifiedName& outVarName)
{
	outType = nullptr;
	if (!platform || mangledName.empty() || (mangledName[0] != '?' && mangledName[0] != '.'))
		return false;
	try
	{
		Demangle demangle(Ref<Platform>(platform), mangledName);
		auto result = demangle.Finalize();
		outType = std::move(result.first);
		outVarName = std::move(result.second);
	}
	catch (DemangleException &e)
	{
		LogDebugForException(e, "Demangling Failed '%s' '%s;", mangledName.c_str(), e.what());
		return false;
	}
	return true;
}

bool Demangle::DemangleMS(Architecture* arch, const string& mangledName, Ref<Type>& outType,
                          QualifiedName& outVarName)
{
	outType = nullptr;
	if (mangledName.empty() || (mangledName[0] != '?' && mangledName[0] != '.'))
		return false;
	try
	{
		thread_local Demangle demangle(arch, mangledName);
		demangle.Reset(arch, mangledName);
		auto result = demangle.Finalize();
		outType = std::move(result.first);
		outVarName = std::move(result.second);
	}
	catch (DemangleException &e)
	{
		LogDebugForException(e, "Demangling Failed '%s' '%s;", mangledName.c_str(), e.what());
		return false;
	}
	return true;
}


bool Demangle::DemangleMS(const string& mangledName, Ref<Type>& outType,
                          QualifiedName& outVarName, const Ref<BinaryView>& view)
{
	outType = nullptr;
	if (mangledName.empty() || (mangledName[0] != '?' && mangledName[0] != '.'))
		return false;
	try
	{
		// Can't use thread_local here — BinaryView overload needs platform/view state
		Demangle demangle(view, mangledName);
		auto result = demangle.Finalize();
		outType = std::move(result.first);
		outVarName = std::move(result.second);
	}
	catch (DemangleException &e)
	{
		LogDebugForException(e, "Demangling Failed '%s' '%s;", mangledName.c_str(), e.what());
		return false;
	}
	return true;
}

bool Demangle::DemangleMS(const string& mangledName, Ref<Type>& outType,
                          QualifiedName& outVarName, BinaryView* view)
{
	outType = nullptr;
	if (!view)
		return false;
	return DemangleMS(mangledName, outType, outVarName, Ref<BinaryView>(view));
}


class MSDemangler: public Demangler
{
public:
	MSDemangler(): Demangler("MS")
	{
	}
	~MSDemangler() override {}

	virtual bool IsMangledString(const string& name) override
	{
		return name[0] == '?';
	}

#ifdef BINARYNINJACORE_LIBRARY
	virtual bool Demangle(Architecture* arch, const string& name, Ref<Type>& outType, QualifiedName& outVarName,
	                      BinaryView* view) override
#else
	virtual bool Demangle(Ref<Architecture> arch, const string& name, Ref<Type>& outType, QualifiedName& outVarName,
	                      Ref<BinaryView> view) override
#endif
	{
		if (view)
			return Demangle::DemangleMS(arch, name, outType, outVarName, view);
		return Demangle::DemangleMS(arch, name, outType, outVarName);
	}
};

extern "C"
{
#ifndef BINARYNINJACORE_LIBRARY
	BN_DECLARE_CORE_ABI_VERSION
#endif

#ifdef BINARYNINJACORE_LIBRARY
	bool DemangleMSVCPluginInit()
#elif defined(DEMO_EDITION)
	bool DemangleMSVCPluginInit()
#else
	BINARYNINJAPLUGIN bool CorePluginInit()
#endif
	{
		static MSDemangler* demangler = new MSDemangler();
		Demangler::Register(demangler);
		return true;
	}
}
