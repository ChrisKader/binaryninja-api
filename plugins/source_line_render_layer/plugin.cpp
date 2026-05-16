#include <binaryninjaapi.h>

using namespace BinaryNinja;


class SourceLineRenderLayer: public RenderLayer
{
	static std::string DisplayPath(const std::string& path)
	{
		size_t pos = path.find_last_of("/\\");
		if (pos == std::string::npos)
			return path;
		return path.substr(pos + 1);
	}

	static bool HasAddressSeparator(const DisassemblyTextLine& line)
	{
		for (const auto& token : line.tokens)
		{
			if (token.type == AddressSeparatorToken)
				return true;
		}
		return false;
	}

	static void ApplyToLine(Ref<DebugInfo> debugInfo, DisassemblyTextLine& line, bool requireAddressSeparator)
	{
		if (line.tokens.empty())
			return;

		if (requireAddressSeparator && !HasAddressSeparator(line))
			return;

		auto sourceLines = debugInfo->GetSourceLinesByAddress(line.addr);
		if (sourceLines.empty())
			return;

		const auto& sourceLine = std::get<1>(sourceLines.front());
		std::string text = "  // " + DisplayPath(sourceLine.sourceFile) + ":" + std::to_string(sourceLine.line);
		if (sourceLine.column != 0)
			text += ":" + std::to_string(sourceLine.column);

		line.tokens.emplace_back(CommentToken, text, line.addr);
	}

	void ApplyToLines(Ref<BasicBlock> block, std::vector<DisassemblyTextLine>& lines, bool requireAddressSeparator = true)
	{
		Ref<DebugInfo> debugInfo = block->GetFunction()->GetView()->GetDebugInfo();
		if (!debugInfo)
			return;

		for (auto& line : lines)
			ApplyToLine(debugInfo, line, requireAddressSeparator);
	}

public:
	SourceLineRenderLayer(): RenderLayer("Source Line Annotations") {}

	void ApplyToDisassemblyBlock(Ref<BasicBlock> block, std::vector<DisassemblyTextLine>& lines) override
	{
		ApplyToLines(block, lines);
	}

	void ApplyToLowLevelILBlock(Ref<BasicBlock> block, std::vector<DisassemblyTextLine>& lines) override
	{
		ApplyToLines(block, lines);
	}

	void ApplyToMediumLevelILBlock(Ref<BasicBlock> block, std::vector<DisassemblyTextLine>& lines) override
	{
		ApplyToLines(block, lines);
	}

	void ApplyToHighLevelILBlock(Ref<BasicBlock> block, std::vector<DisassemblyTextLine>& lines) override
	{
		ApplyToLines(block, lines, false);
	}

	void ApplyToHighLevelILBody(Ref<Function> function, std::vector<LinearDisassemblyLine>& lines) override
	{
		Ref<DebugInfo> debugInfo = function->GetView()->GetDebugInfo();
		if (!debugInfo)
			return;

		for (auto& line : lines)
		{
			if (line.type != CodeDisassemblyLineType)
				continue;

			ApplyToLine(debugInfo, line.contents, false);
		}
	}
};


extern "C" {
	BN_DECLARE_CORE_ABI_VERSION

#ifdef DEMO_EDITION
	bool SourceLineRenderLayerPluginInit()
#else
	BINARYNINJAPLUGIN bool CorePluginInit()
#endif
	{
		static SourceLineRenderLayer* layer = new SourceLineRenderLayer();
		RenderLayer::Register(layer, DisabledByDefaultRenderLayerDefaultEnableState);
		return true;
	}
}
