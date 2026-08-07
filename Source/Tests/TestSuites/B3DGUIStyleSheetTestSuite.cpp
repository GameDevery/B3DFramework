//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#include "B3DGUIStyleSheetTestSuite.h"

#include "GUI/B3DGUIUtility.h"
#include "GUI/StyleSheet/B3DGUIStyleSheetParser.h"
#include "Resources/B3DBuiltinResources.h"
#include "String/B3DSourceCode.h"

using namespace b3d;

namespace
{
	/** Style sheet exercising a base rule, two single pseudo-class rules and a chained pseudo-class rule on the same element. */
	constexpr const char* kChainedPseudoClassSource =
		"button\n"
		"{\n"
		"    background-color: #101010;\n"
		"}\n"
		"\n"
		"button:hover\n"
		"{\n"
		"    background-color: #202020;\n"
		"}\n"
		"\n"
		"button:checked\n"
		"{\n"
		"    background-color: #303030;\n"
		"}\n"
		"\n"
		"button:hover:checked\n"
		"{\n"
		"    background-color: #404040;\n"
		"}\n";

	/** Style sheet exercising the disabled state alongside the interaction states it must win over. */
	constexpr const char* kDisabledStateSource =
		"button\n"
		"{\n"
		"    background-color: #101010;\n"
		"}\n"
		"\n"
		"button:hover\n"
		"{\n"
		"    background-color: #202020;\n"
		"}\n"
		"\n"
		"button:disabled\n"
		"{\n"
		"    background-color: #303030;\n"
		"}\n";

	/** Parses the provided style sheet source. Returns null if parsing failed. */
	TShared<GUIStyleSheet> ParseStyleSheet(const char* source)
	{
		GUIStyleSheetParser parser;

		return parser.Parse(B3DMakeShared<SourceCode>(String(source)));
	}

	/** Parses the chained pseudo-class style sheet. Returns null if parsing failed. */
	TShared<GUIStyleSheet> ParseChainedPseudoClassStyleSheet()
	{
		return ParseStyleSheet(kChainedPseudoClassSource);
	}

	/** Returns the rules of the first (and expected to be only) ruleset in the style sheet. */
	const GUIStyleSheetRules& GetOnlyRules(const GUIStyleSheet& styleSheet)
	{
		return styleSheet.GetRulesets().front().Rules;
	}

	/** Returns the ruleset whose selector list matches @p uniqueName (e.g. "button:hover:checked"), or null if there is none. */
	const GUIStyleSheetRuleset* FindRuleset(const GUIStyleSheet& styleSheet, StringView uniqueName)
	{
		for(const GUIStyleSheetRuleset& ruleset : styleSheet.GetRulesets())
		{
			if(ruleset.SelectorList.GetUniqueName() == uniqueName)
				return &ruleset;
		}

		return nullptr;
	}

	/**
	 * Builds the state-ruleset set a GUI element matching every ruleset in @p styleSheet would receive. This mirrors what
	 * GUIStyleSheetCascade::BuildStateRulesets() produces for an element that all the sheet's selectors apply to, without
	 * requiring a live GUI element.
	 */
	GUIStyleSheetStateRulesets BuildAllMatchingStateRulesets(const TShared<GUIStyleSheet>& styleSheet)
	{
		GUIStyleSheetStateRulesets::StyleSheetRulesetIndices styleSheetRulesetIndices;
		styleSheetRulesetIndices.StyleSheet = styleSheet;

		// Rulesets are sorted from least to most specific by GUIStyleSheet::Initialize(), so ascending index order is also
		// the order in which the rules must be overridden
		for(u32 rulesetIndex = 0; rulesetIndex < (u32)styleSheet->GetRulesets().Size(); ++rulesetIndex)
			styleSheetRulesetIndices.RulesetIndices.Add(rulesetIndex);

		GUIStyleSheetStateRulesets stateRulesets;
		stateRulesets.StyleSheets.Add(styleSheetRulesetIndices);

		return stateRulesets;
	}
} // anonymous namespace

GUIStyleSheetTestSuite::GUIStyleSheetTestSuite()
	: TestSuite("GUIStyleSheetTestSuite")
{
	B3D_ADD_TEST(GUIStyleSheetTestSuite::TestChainedPseudoClassParsing)
	B3D_ADD_TEST(GUIStyleSheetTestSuite::TestChainedPseudoClassSpecificity)
	B3D_ADD_TEST(GUIStyleSheetTestSuite::TestChainedPseudoClassStateResolution)
	B3D_ADD_TEST(GUIStyleSheetTestSuite::TestDisabledStateResolution)
	B3D_ADD_TEST(GUIStyleSheetTestSuite::TestComments)
	B3D_ADD_TEST(GUIStyleSheetTestSuite::TestUnexpectedTokenIsReportedAsError)
	B3D_ADD_TEST(GUIStyleSheetTestSuite::TestTextMetricParsing)
	B3D_ADD_TEST(GUIStyleSheetTestSuite::TestFontWeightSelectsBoldFace)
	B3D_ADD_TEST(GUIStyleSheetTestSuite::TestTextMetricsAffectMeasuredSize)
}

void GUIStyleSheetTestSuite::TestChainedPseudoClassParsing()
{
	const TShared<GUIStyleSheet> styleSheet = ParseChainedPseudoClassStyleSheet();
	B3D_TEST_ASSERT(styleSheet != nullptr)
	if(styleSheet == nullptr)
		return;

	B3D_TEST_ASSERT((u32)styleSheet->GetRulesets().Size() == 4)

	const GUIStyleSheetRuleset* chainedRuleset = FindRuleset(*styleSheet, "button:hover:checked");
	B3D_TEST_ASSERT(chainedRuleset != nullptr)
	if(chainedRuleset == nullptr)
		return;

	// A chained selector must be kept as separate selectors, not folded into a single pseudo-class
	const auto& selectors = chainedRuleset->SelectorList.Selectors;
	B3D_TEST_ASSERT((u32)selectors.Size() == 3)
	if((u32)selectors.Size() != 3)
		return;

	B3D_TEST_ASSERT(selectors[0].SelectorType == GUIStyleSheetSelectorType::Element)
	B3D_TEST_ASSERT(selectors[0].Name == "button")
	B3D_TEST_ASSERT(selectors[0].CombinatorType == GUIStyleSheetCombinatorType::None)

	B3D_TEST_ASSERT(selectors[1].SelectorType == GUIStyleSheetSelectorType::PseudoClass)
	B3D_TEST_ASSERT(selectors[1].Name == "hover")
	B3D_TEST_ASSERT(selectors[1].CombinatorType == GUIStyleSheetCombinatorType::None)

	B3D_TEST_ASSERT(selectors[2].SelectorType == GUIStyleSheetSelectorType::PseudoClass)
	B3D_TEST_ASSERT(selectors[2].Name == "checked")
	B3D_TEST_ASSERT(selectors[2].CombinatorType == GUIStyleSheetCombinatorType::None)
}

void GUIStyleSheetTestSuite::TestChainedPseudoClassSpecificity()
{
	const TShared<GUIStyleSheet> styleSheet = ParseChainedPseudoClassStyleSheet();
	B3D_TEST_ASSERT(styleSheet != nullptr)
	if(styleSheet == nullptr)
		return;

	const GUIStyleSheetRuleset* baseRuleset = FindRuleset(*styleSheet, "button");
	const GUIStyleSheetRuleset* hoverRuleset = FindRuleset(*styleSheet, "button:hover");
	const GUIStyleSheetRuleset* chainedRuleset = FindRuleset(*styleSheet, "button:hover:checked");

	B3D_TEST_ASSERT(baseRuleset != nullptr && hoverRuleset != nullptr && chainedRuleset != nullptr)
	if(baseRuleset == nullptr || hoverRuleset == nullptr || chainedRuleset == nullptr)
		return;

	// Every chained pseudo-class must add specificity, so a chained rule always wins over its single-pseudo-class counterparts
	B3D_TEST_ASSERT(hoverRuleset->SelectorList.CalculateSpecificity() > baseRuleset->SelectorList.CalculateSpecificity())
	B3D_TEST_ASSERT(chainedRuleset->SelectorList.CalculateSpecificity() > hoverRuleset->SelectorList.CalculateSpecificity())
}

void GUIStyleSheetTestSuite::TestChainedPseudoClassStateResolution()
{
	const TShared<GUIStyleSheet> styleSheet = ParseChainedPseudoClassStyleSheet();
	B3D_TEST_ASSERT(styleSheet != nullptr)
	if(styleSheet == nullptr)
		return;

	const GUIStyleSheetRuleset* baseRuleset = FindRuleset(*styleSheet, "button");
	const GUIStyleSheetRuleset* hoverRuleset = FindRuleset(*styleSheet, "button:hover");
	const GUIStyleSheetRuleset* checkedRuleset = FindRuleset(*styleSheet, "button:checked");
	const GUIStyleSheetRuleset* chainedRuleset = FindRuleset(*styleSheet, "button:hover:checked");

	B3D_TEST_ASSERT(baseRuleset != nullptr && hoverRuleset != nullptr && checkedRuleset != nullptr && chainedRuleset != nullptr)
	if(baseRuleset == nullptr || hoverRuleset == nullptr || checkedRuleset == nullptr || chainedRuleset == nullptr)
		return;

	const GUIStyleSheetStateRulesets stateRulesets = BuildAllMatchingStateRulesets(styleSheet);

	auto fnResolveBackgroundColor = [&stateRulesets](GUIElementStates state)
	{
		return stateRulesets.BuildStateRuleset(state)->Rules.BackgroundColor;
	};

	// Unrelated states fall back to the base rule
	B3D_TEST_ASSERT(fnResolveBackgroundColor(GUIElementState::Normal) == baseRuleset->Rules.BackgroundColor)
	B3D_TEST_ASSERT(fnResolveBackgroundColor(GUIElementState::Active) == baseRuleset->Rules.BackgroundColor)

	// A single active state must not activate the chained rule
	B3D_TEST_ASSERT(fnResolveBackgroundColor(GUIElementState::Hover) == hoverRuleset->Rules.BackgroundColor)
	B3D_TEST_ASSERT(fnResolveBackgroundColor(GUIElementState::Checked) == checkedRuleset->Rules.BackgroundColor)

	// Both states active: the chained rule wins over both single-state rules
	B3D_TEST_ASSERT(fnResolveBackgroundColor(GUIElementState::Hover | GUIElementState::Checked) == chainedRuleset->Rules.BackgroundColor)

	// Additional unrelated states must not disturb the chained match
	B3D_TEST_ASSERT(fnResolveBackgroundColor(GUIElementState::Hover | GUIElementState::Checked | GUIElementState::Focus) == chainedRuleset->Rules.BackgroundColor)
	B3D_TEST_ASSERT(fnResolveBackgroundColor(GUIElementState::Hover | GUIElementState::Focus) == hoverRuleset->Rules.BackgroundColor)
}

void GUIStyleSheetTestSuite::TestDisabledStateResolution()
{
	const TShared<GUIStyleSheet> styleSheet = ParseStyleSheet(kDisabledStateSource);
	B3D_TEST_ASSERT(styleSheet != nullptr)
	if(styleSheet == nullptr)
		return;

	const GUIStyleSheetRuleset* baseRuleset = FindRuleset(*styleSheet, "button");
	const GUIStyleSheetRuleset* hoverRuleset = FindRuleset(*styleSheet, "button:hover");
	const GUIStyleSheetRuleset* disabledRuleset = FindRuleset(*styleSheet, "button:disabled");

	B3D_TEST_ASSERT(baseRuleset != nullptr && hoverRuleset != nullptr && disabledRuleset != nullptr)
	if(baseRuleset == nullptr || hoverRuleset == nullptr || disabledRuleset == nullptr)
		return;

	const GUIStyleSheetStateRulesets stateRulesets = BuildAllMatchingStateRulesets(styleSheet);

	auto fnResolveBackgroundColor = [&stateRulesets](GUIElementStates state)
	{
		return stateRulesets.BuildStateRuleset(state)->Rules.BackgroundColor;
	};

	B3D_TEST_ASSERT(fnResolveBackgroundColor(GUIElementState::Normal) == baseRuleset->Rules.BackgroundColor)
	B3D_TEST_ASSERT(fnResolveBackgroundColor(GUIElementState::Hover) == hoverRuleset->Rules.BackgroundColor)
	B3D_TEST_ASSERT(fnResolveBackgroundColor(GUIElementState::Disabled) == disabledRuleset->Rules.BackgroundColor)

	// A disabled element must not pick up any interaction visuals, no matter which interaction states are still set
	B3D_TEST_ASSERT(fnResolveBackgroundColor(GUIElementState::Disabled | GUIElementState::Hover) == disabledRuleset->Rules.BackgroundColor)
	B3D_TEST_ASSERT(fnResolveBackgroundColor(GUIElementState::Disabled | GUIElementState::Active | GUIElementState::Focus) == disabledRuleset->Rules.BackgroundColor)

	// The checked state survives, as it describes what the element represents rather than an interaction in progress.
	// State normalization is internal to BuildStateRuleset, so equal normalized states must yield the identical cached ruleset.
	B3D_TEST_ASSERT(stateRulesets.BuildStateRuleset(GUIElementState::Disabled | GUIElementState::Hover | GUIElementState::Active | GUIElementState::Focus | GUIElementState::Checked) ==
		stateRulesets.BuildStateRuleset(GUIElementState::Disabled | GUIElementState::Checked))
}

void GUIStyleSheetTestSuite::TestComments()
{
	constexpr const char* kCommentedSource =
		"/* Leading block comment */\n"
		"// Leading line comment\n"
		"button /* comment between selectors */ label\n"
		"{\n"
		"    /* comment before a property */\n"
		"    background-color: #101010; // trailing line comment\n"
		"    color: /* comment inside a declaration */ #202020;\n"
		"}\n"
		"/* Trailing block\n"
		"   spanning multiple lines */\n";

	const TShared<GUIStyleSheet> styleSheet = ParseStyleSheet(kCommentedSource);
	B3D_TEST_ASSERT(styleSheet != nullptr)
	if(styleSheet == nullptr)
		return;

	B3D_TEST_ASSERT((u32)styleSheet->GetRulesets().Size() == 1)
	if((u32)styleSheet->GetRulesets().Size() != 1)
		return;

	const GUIStyleSheetRuleset& ruleset = styleSheet->GetRulesets().front();
	B3D_TEST_ASSERT(ruleset.Rules.IsPropertySet(GUIStyleSheetPropertyType::BackgroundColor))
	B3D_TEST_ASSERT(ruleset.Rules.IsPropertySet(GUIStyleSheetPropertyType::Color))

	// A comment between two selectors separates them the same way whitespace does, so it must still read as a descendant selector
	B3D_TEST_ASSERT((u32)ruleset.SelectorList.Selectors.Size() == 2)
	if((u32)ruleset.SelectorList.Selectors.Size() != 2)
		return;

	B3D_TEST_ASSERT(ruleset.SelectorList.Selectors[0].Name == "button")
	B3D_TEST_ASSERT(ruleset.SelectorList.Selectors[0].CombinatorType == GUIStyleSheetCombinatorType::AncestorOf)
	B3D_TEST_ASSERT(ruleset.SelectorList.Selectors[1].Name == "label")

	// A slash is still a token of its own, as it separates the alpha channel in a color function
	const TShared<GUIStyleSheet> slashStyleSheet = ParseStyleSheet("button\n{\n    color: rgb(255, 128, 0 / 0.5);\n}\n");
	B3D_TEST_ASSERT(slashStyleSheet != nullptr)
	if(slashStyleSheet != nullptr)
		B3D_TEST_ASSERT(Math::ApproxEquals(GetOnlyRules(*slashStyleSheet).Color.A, 0.5f))

	// An unterminated block comment is a lexer error rather than a silently truncated file
	GUIStyleSheetParser parser;
	const TShared<GUIStyleSheet> unterminatedStyleSheet = parser.Parse(B3DMakeShared<SourceCode>(String("button\n{\n    color: #101010;\n}\n/* never closed\n")));
	B3D_TEST_ASSERT(unterminatedStyleSheet == nullptr)
	B3D_TEST_ASSERT(!parser.GetErrors().empty())
}

void GUIStyleSheetTestSuite::TestUnexpectedTokenIsReportedAsError()
{
	// A token the ruleset body cannot consume must produce a parse error, not spin the parser
	GUIStyleSheetParser parser;
	const TShared<GUIStyleSheet> styleSheet = parser.Parse(B3DMakeShared<SourceCode>(String("button\n{\n    background-color: #101010;\n    ;\n}\n")));

	B3D_TEST_ASSERT(styleSheet == nullptr)
	B3D_TEST_ASSERT(!parser.GetErrors().empty())
}

void GUIStyleSheetTestSuite::TestTextMetricParsing()
{
	const TShared<GUIStyleSheet> styleSheet = ParseStyleSheet(
		"label\n"
		"{\n"
		"    line-height: 1.35;\n"
		"    letter-spacing: -0.2px;\n"
		"    font-weight: bold;\n"
		"}\n");

	B3D_TEST_ASSERT(styleSheet != nullptr)
	if(styleSheet == nullptr)
		return;

	const GUIStyleSheetRules& rules = GetOnlyRules(*styleSheet);
	B3D_TEST_ASSERT(rules.IsPropertySet(GUIStyleSheetPropertyType::LineHeight))
	B3D_TEST_ASSERT(rules.IsPropertySet(GUIStyleSheetPropertyType::LetterSpacing))
	B3D_TEST_ASSERT(rules.IsPropertySet(GUIStyleSheetPropertyType::FontWeight))

	B3D_TEST_ASSERT(Math::ApproxEquals(rules.LineHeight, 1.35f))
	B3D_TEST_ASSERT(Math::ApproxEquals(rules.LetterSpacing, -0.2f))
	B3D_TEST_ASSERT(rules.FontWeight == GUIFontWeight::Bold)

	// Metrics are in logical units, so they must be scaled along with the font size
	const TextMetrics scaledMetrics = rules.GetTextMetrics(2.0f);
	B3D_TEST_ASSERT(Math::ApproxEquals(scaledMetrics.LineHeight, 1.35f))
	B3D_TEST_ASSERT(Math::ApproxEquals(scaledMetrics.LetterSpacing, -0.4f))

	// Defaults must leave the metrics reported by the font untouched
	const TShared<GUIStyleSheet> defaultStyleSheet = ParseStyleSheet("label\n{\n    color: #101010;\n}\n");
	B3D_TEST_ASSERT(defaultStyleSheet != nullptr)
	if(defaultStyleSheet != nullptr)
	{
		const GUIStyleSheetRules& defaultRules = GetOnlyRules(*defaultStyleSheet);
		B3D_TEST_ASSERT(defaultRules.GetTextMetrics() == TextMetrics())
		B3D_TEST_ASSERT(defaultRules.FontWeight == GUIFontWeight::Normal)
	}
}

void GUIStyleSheetTestSuite::TestFontWeightSelectsBoldFace()
{
	const HFont regularFace = GetBuiltinResources().GetDefaultFont();
	B3D_TEST_ASSERT(regularFace != nullptr)
	if(regularFace == nullptr)
		return;

	GUIStyleSheetRules rules;
	rules.Font = regularFace;

	// Without a registered bold face the regular one is used, so a missing face never blanks out the text
	rules.FontWeight = GUIFontWeight::Bold;
	B3D_TEST_ASSERT(rules.GetWeightedFont() == regularFace)

	const HFont boldFace = GetBuiltinResources().GetBoldFont("Inter");
	if(boldFace != nullptr)
	{
		rules.BoldFont = boldFace;
		B3D_TEST_ASSERT(rules.GetWeightedFont() == boldFace)

		rules.FontWeight = GUIFontWeight::Normal;
		B3D_TEST_ASSERT(rules.GetWeightedFont() == regularFace)
	}
}

void GUIStyleSheetTestSuite::TestTextMetricsAffectMeasuredSize()
{
	const HFont font = GetBuiltinResources().GetDefaultFont();
	B3D_TEST_ASSERT(font != nullptr)
	if(font == nullptr)
		return;

	const String text = "Measured text";

	GUIStyleSheetRules rules;
	rules.Font = font;
	rules.FontSize = 12.0f;

	const GUILogicalSize defaultSize = GUIUtility::CalculateOptimalContentSizeWithPaddingAndBorder(text, rules, 0);
	B3D_TEST_ASSERT(defaultSize.Width > 0 && defaultSize.Height > 0)

	// Letter spacing widens the text without changing its height
	rules.LetterSpacing = 4.0f;
	const GUILogicalSize trackedSize = GUIUtility::CalculateOptimalContentSizeWithPaddingAndBorder(text, rules, 0);

	B3D_TEST_ASSERT(trackedSize.Width > defaultSize.Width)
	B3D_TEST_ASSERT(trackedSize.Height == defaultSize.Height)

	// Negative letter spacing tightens it again
	rules.LetterSpacing = -1.0f;
	const GUILogicalSize tightenedSize = GUIUtility::CalculateOptimalContentSizeWithPaddingAndBorder(text, rules, 0);
	B3D_TEST_ASSERT(tightenedSize.Width < defaultSize.Width)

	// Line height scales the height without changing the width
	rules.LetterSpacing = 0.0f;
	rules.LineHeight = 2.0f;
	const GUILogicalSize spacedSize = GUIUtility::CalculateOptimalContentSizeWithPaddingAndBorder(text, rules, 0);

	B3D_TEST_ASSERT(spacedSize.Height > defaultSize.Height)
	B3D_TEST_ASSERT(spacedSize.Width == defaultSize.Width)
}
