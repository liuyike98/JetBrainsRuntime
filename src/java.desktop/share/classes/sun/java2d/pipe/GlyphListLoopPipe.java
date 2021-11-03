/*
 * Copyright (c) 2005, 2016, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package sun.java2d.pipe;

import sun.awt.SunHints;
import sun.java2d.SunGraphics2D;
import sun.font.GlyphList;

/**
 * A delegate pipe of SG2D which implements redispatching of
 * for the src mode loops in the drawGlyphVector case where
 * the installed loop may not match the glyphvector.
 */

public abstract class GlyphListLoopPipe extends GlyphListPipe
    implements LoopBasedPipe
{
    protected void drawGlyphList(SunGraphics2D sg2d, GlyphList gl,
                                 int aaHint) {
        int prevBorder = 0;
        int pixelFormat = -1;
        int len = gl.getNumGlyphs();
        gl.startGlyphIteration();
        for (int i = 0; i < len; i++) {
            int newFormat = gl.getPixelFormat(i);
            if (newFormat != pixelFormat) {
                if (pixelFormat != -1) drawGlyphListSegment(sg2d, gl, prevBorder, i, aaHint, pixelFormat);
                prevBorder = i;
                pixelFormat = newFormat;
            }
        }
        drawGlyphListSegment(sg2d, gl, prevBorder, len, aaHint, pixelFormat);
    }

    private void drawGlyphListSegment(SunGraphics2D sg2d, GlyphList gl, int fromglyph, int toGlyph,
                                      int aaHint, int pixelFormat) {
        if (fromglyph >= toGlyph) return;
        switch (pixelFormat) {
            case 1:
                if (aaHint == SunHints.INTVAL_TEXT_ANTIALIAS_OFF) {
                    sg2d.loops.drawGlyphListLoop.
                            DrawGlyphList(sg2d, sg2d.surfaceData, gl, fromglyph, toGlyph);
                } else {
                    sg2d.loops.drawGlyphListAALoop.
                            DrawGlyphListAA(sg2d, sg2d.surfaceData, gl, fromglyph, toGlyph);
                }
                return;
            case 3:
                sg2d.loops.drawGlyphListLCDLoop.
                        DrawGlyphListLCD(sg2d, sg2d.surfaceData, gl, fromglyph, toGlyph);
                return;
            case 4:
                sg2d.loops.drawGlyphListColorLoop.
                        DrawGlyphListColor(sg2d, sg2d.surfaceData, gl, fromglyph, toGlyph);
                return;
        }
    }
}
