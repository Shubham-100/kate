/* This file is part of the KDE libraries
   Copyright (c) 2000 Waldo Bastian <bastian@kde.org>
   Copyright (C) 2002, 2003 Christoph Cullmann <cullmann@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef _KATE_BUFFER_H_
#define _KATE_BUFFER_H_

#include "katetextline.h"
#include "katecodefoldinghelpers.h"

#include <kvmallocator.h>

#include <qptrlist.h>
#include <qobject.h>
#include <qtimer.h>

class KateLineInfo;
class KateBufBlock;
class KateDocument;
class Highlight;

class QTextCodec;

/**
 * list which allows O(1) inserts/removes
 * will not delete the elements on remove
 * will use the next/prevNode pointers in the KateBufBlocks !
 * internal use: loaded/clean/dirty block lists
 */
class KateBufBlockList
{
  public:
    KateBufBlockList ();
    ~KateBufBlockList ();
    
  public:
    /**
     * count of blocks in this list
     */
    inline uint count() const { return m_count; }
  
    /**
     * first block in this list or 0
     */
    inline KateBufBlock *first () { return m_first; };
    
    /**
     * append a block to this list !
     * will remove it from the list it belonged before !
     */
    void append (KateBufBlock *buf);
    
    /**
     * remove the block from the list it belongs to !
     */
    static void remove (KateBufBlock *buf);
    
  private:
    void removeInternal (KateBufBlock *buf);
    
  private:
    uint m_count;
    KateBufBlock *m_first;
    KateBufBlock *m_last;
};

/**
 * The KateBuffer class maintains a collections of lines.
 * It allows to maintain state information in a lazy way.
 * It handles swapping out of data using secondary storage.
 *
 * It is designed to handle large amounts of text-data efficiently
 * with respect to CPU and memory usage.
 *
 * @author Waldo Bastian <bastian@kde.org>
 */
class KateBuffer : public QObject
{
  Q_OBJECT
  
  friend class KateBufBlock;

  public:
    /**
     * Create an empty buffer.
     */
    KateBuffer(KateDocument *doc);

    /**
     * Goodbye buffer
     */
    ~KateBuffer();
    
  public slots:
    /**
     * change the visibility of a given line
     */
    void setLineVisible (unsigned int lineNr, bool visible);

  public:
    KateDocument* document() const { return m_doc; }
    
    /**
     * Open a file, use the given filename + codec (internal use of qtextstream)
     */
    bool openFile (const QString &m_file);

    /**
     * Can the current codec handle all chars
     */
    bool canEncode ();

    /**
     * Save the buffer to a file, use the given filename + codec + end of line chars (internal use of qtextstream)
     */
    bool saveFile (const QString &m_file);

    /**
     * Return the total number of lines in the buffer.
     */
    inline uint count() const
    {
      return m_lines;
    }

    uint countVisible ();

    uint lineNumber (uint visibleLine);

    uint lineVisibleNumber (uint line);

    void lineInfo (KateLineInfo *info, unsigned int line);

    KateCodeFoldingTree *foldingTree ();

    inline void setHlUpdate (bool b)
    {
      m_hlUpdate = b;
    }

    void dumpRegionTree ();

    /**
     * Return line @p i
     */
    TextLine::Ptr line(uint i);

    /**
     * Return line @p i without triggering highlighting
     */
    TextLine::Ptr plainLine(uint i);
    
    /**
     * Return textline @p i without triggering highlighting
     */
    QString textLine(uint i);

    /**
     * Insert @p line in front of line @p i
     */
    void insertLine(uint i, TextLine::Ptr line);

    /**
     * Remove line @p i
     */
    void removeLine(uint i);

    /**
     * Change line @p i
     */
    void changeLine(uint i);

    /**
     * Clear the buffer.
     */
    void clear();

    /**
     * Use @p highlight for highlighting
     *
     * @p highlight may be 0 in which case highlighting
     * will be disabled.
     */
    void setHighlight (Highlight *highlight);

    Highlight *highlight () { return m_highlight; };

    /**
     * Update the highlighting.
     *
     * PRE-condition:
     *   All lines prior to @p from have been highlighted already.
     *
     * POST-condition:
     *   All lines till at least @p to haven been highlighted.
     */
    void updateHighlighting(uint from, uint to, bool invalidate);

    /**
     * Invalidate highlighting of whole buffer.
     */
    void invalidateHighlighting();

    /**
     * Get the whole text in the buffer as a string.
     */
    QString text();

    /**
     * Get the text between the two given positions.
     */
    QString text(uint startLine, uint startCol, uint endLine, uint endCol, bool blockwise = false);

    uint length ();

    int lineLength ( uint line );

    /**
     * was the last loading broken because of not enough tmp disk space ?
     * (will be reseted on successful save of the file, user gets warning if he really wants to do it)
     */
    bool loadingBorked () const
    {
      return m_loadingBorked;
    }

    void setTabWidth (uint w);
    
    inline uint tabWidth () const { return m_tabWidth; }
    
    inline KVMAllocator *vm () { return &m_vm; }

  private:
    /**
     * Find the block containing line @p i
     * index pointer gets filled with index of block in m_blocks
     * index only valid if returned block != 0 !
     */
    KateBufBlock *findBlock (uint i, uint *index = 0);

    /**
     * Highlight information needs to be updated.
     *
     * @param buf The buffer being processed.
     * @param startState highlighting state of last line before range
     * @param from first line in range
     * @param to last line in range
     *
     * @returns true when the highlighting in the next block needs to be updated,
     * false otherwise.
     */
    bool needHighlight(KateBufBlock *buf, uint from, uint to);

    void pleaseHighlight (uint,uint);

  private slots:
    void pleaseHighlight ();
    
  signals:
    /**
     * Emittend if codefolding returned with a changed list
     */
    void codeFoldingUpdated();

    /**
     * Emitted when the highlighting of a certain range has
     * changed.
     */
    void tagLines(int start, int end);

  private:
    /**
     * document we belong too
     */
    KateDocument *m_doc;
  
    /**
     * current line count
     */
    uint m_lines;
    
    /**
     * ALL blocks
     * in order of linenumbers
     */
    QValueVector<KateBufBlock*> m_blocks;
    
    /**
     * last block where the start/end line is in sync with real life
     */
    uint m_lastInSyncBlock;
    
    /**
     * last block found by findBlock, there to make searching faster
     */
    uint m_lastFoundBlock;

    /**
     * vm allocator
     */
    KVMAllocator m_vm;

    /**
     * status of the cache read/write errors
     * write errors get handled, read errors not really atm
     */
    bool m_cacheReadError;
    bool m_cacheWriteError;
    
    /**
     * had we cache error while loading ?
     */
    bool m_loadingBorked;

  /**
   * highlighting & folding relevant stuff
   */
  private:
    /**
     * current highlighting mode or 0
     */
    Highlight *m_highlight;
    
    /**
     * highlighting timer
     */
    QTimer m_highlightTimer;
    
    /**
     * folding tree
     */
    KateCodeFoldingTree m_regionTree;
    
    /**
     * The highest line with correct highlight info
     */
    uint m_highlightedTo;
    
    /**
     * The highest line that we requested highlight for
     */ 
    uint m_highlightedRequested;

    /**
     * enable/disable hl updates
     */
    bool m_hlUpdate;
    
    // for the scrapty indent sensitive langs
    uint m_tabWidth;
    
    uint m_highlightedTill;
    uint m_highlightedEnd;
    uint m_highlightedSteps;
  
  /**
   * only used from the KateBufBlocks !
   * lists to keep track of how much blocks are floating around
   * in the different states
   */
  private:
    /**
     * list of blocks that can be swapped out.
     */
    KateBufBlockList m_loadedBlocks;

    /**
     * list of blocks that can be disposed.
     */
    KateBufBlockList m_cleanBlocks;

    /**
     * list of blocks that are dirty.
     */
    KateBufBlockList m_dirtyBlocks;
};

#endif
