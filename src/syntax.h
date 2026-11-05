// syntax.h - C/C++ syntax highlighting and read-only code block rendering
#pragma once
#include "common.h"

namespace syntax {

enum Kind : unsigned char { Plain = 0, Keyword, Type, Number, String, Comment, Preproc, Function, Punct };

// Classify each character of a line. `inBlock` carries /* */ state across lines.
void Classify(const std::string& line, bool& inBlock, std::vector<unsigned char>& kinds);
Color ColorFor(unsigned char k);

// Draw a highlighted, boxed code block. The font shrinks (down to minSize) so the
// longest line fits `width`. Returns the height used.
float DrawCode(const std::string& code, float x, float y, float width, float size, bool draw = true,
               bool lineNumbers = false, float minSize = 12.f);
// Plain monospace "console output" block
float DrawOutput(const std::string& text, float x, float y, float width, float size, bool draw = true);

} // namespace syntax
