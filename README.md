# Good fonts for XCB #

A project about creating something similar to Xft but for the XCB.

Project roadmap:

* Font matching phase (fontconfig): creation of a string that can be used.

* Font rasterizing phase (freetype): use the pattern given from fontconfig
  to rasterize from the font file given

* Join both inside an XCB wrapper
