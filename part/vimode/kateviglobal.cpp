/*  This file is part of the KDE libraries and the Kate part.
 *
 *  Copyright (C) 2008-2011 Erlend Hamberg <ehamberg@gmail.com>
 *  Copyright (C) 2011 Svyatoslav Kuzmich <svatoslav1@gmail.com>
 *  Copyright (C) 2012 - 2013 Simon St James <kdedevel@etotheipiplusone.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include <stdio.h>

#include "kateviglobal.h"
#include "katevikeyparser.h"

#include "kconfiggroup.h"
#include "kdebug.h"
#include <ktexteditor/movingcursor.h>
#include <QApplication>
#include <QClipboard>

KateViGlobal::KateViGlobal()
{
}

KateViGlobal::~KateViGlobal()
{
}

void KateViGlobal::writeConfig( KConfigGroup &config ) const
{
  config.writeEntry( "Normal Mode Mapping Keys", getMappings( NormalMode, true ) );
  QStringList l;
  QList<bool> isRecursive;
  foreach( const QString &s, getMappings( NormalMode ) ) {
    l << KateViKeyParser::self()->decodeKeySequence( getMapping( NormalMode, s ) );
    isRecursive << isMappingRecursive( NormalMode, s );
  }
  config.writeEntry( "Normal Mode Mappings", l );
  config.writeEntry( "Normal Mode Mappings Recursion", isRecursive );
}

void KateViGlobal::readConfig( const KConfigGroup &config )
{
    QStringList keys = config.readEntry( "Normal Mode Mapping Keys", QStringList() );
    QStringList mappings = config.readEntry( "Normal Mode Mappings", QStringList() );
    QList<bool> isRecursive = config.readEntry( "Normal Mode Mappings Recursion", QList<bool>());

    // sanity check
    if ( keys.length() == mappings.length() ) {
      for ( int i = 0; i < keys.length(); i++ ) {
        // "Recursion" is a newly-introduced part of the config that some users won't have,
        // so rather than abort (and lose our mappings) if there are not enough entries, simply
        // treat any missing ones as Recursive (for backwards compatibility).
        KateViModeBase::MappingRecursion recursion = KateViModeBase::Recursive;
        if (isRecursive.size() > i && !isRecursive.at(i))
        {
          recursion = KateViModeBase::NonRecursive;
        }
        addMapping( NormalMode, keys.at( i ), mappings.at( i ), recursion);
        kDebug( 13070 ) << "Mapping " << keys.at( i ) << " -> " << mappings.at( i );
      }
    } else {
      kDebug( 13070 ) << "Error when reading mappings from config: number of keys != number of values";
    }
}

KateViRegister KateViGlobal::getRegister( const QChar &reg ) const
{
  KateViRegister regPair;
  QChar _reg = ( reg != '"' ? reg : m_defaultRegister );

  if ( _reg >= '1' && _reg <= '9' ) { // numbered register
    int index = QString( _reg ).toInt()-1;
    if ( m_numberedRegisters.size() > index) {
      regPair = m_numberedRegisters.at( index );
    }
  } else if ( _reg == '+' ) { // system clipboard register
      QString regContent = QApplication::clipboard()->text( QClipboard::Clipboard );
      regPair = KateViRegister( regContent, CharWise );
  } else if ( _reg == '*' ) { // system selection register
      QString regContent = QApplication::clipboard()->text( QClipboard::Selection );
      regPair = KateViRegister( regContent, CharWise );
  } else { // regular, named register
    if ( m_registers.contains( _reg ) ) {
      regPair = m_registers.value( _reg );
    }
  }

  return regPair;
}

QString KateViGlobal::getRegisterContent( const QChar &reg ) const
{
  return getRegister( reg ).first;
}

OperationMode KateViGlobal::getRegisterFlag( const QChar &reg ) const
{
  return getRegister( reg ).second;
}

void KateViGlobal::addToNumberedRegister( const QString &text, OperationMode flag )
{
  if ( m_numberedRegisters.size() == 9 ) {
    m_numberedRegisters.removeLast();
  }

  // register 0 is used for the last yank command, so insert at position 1
  m_numberedRegisters.prepend( KateViRegister(text, flag ) );

  kDebug( 13070 ) << "Register 1-9:";
  for ( int i = 0; i < m_numberedRegisters.size(); i++ ) {
      kDebug( 13070 ) << "\t Register " << i+1 << ": " << m_numberedRegisters.at( i );
  }
}

void KateViGlobal::fillRegister( const QChar &reg, const QString &text, OperationMode flag )
{
  // the specified register is the "black hole register", don't do anything
  if ( reg == '_' ) {
    return;
  }

  if ( reg >= '1' && reg <= '9' ) { // "kill ring" registers
    addToNumberedRegister( text );
  } else if ( reg == '+' ) { // system clipboard register
      QApplication::clipboard()->setText( text,  QClipboard::Clipboard );
  } else if ( reg == '*' ) { // system selection register
      QApplication::clipboard()->setText( text, QClipboard::Selection );
  } else {
    m_registers.insert( reg, KateViRegister(text, flag) );
  }

  kDebug( 13070 ) << "Register " << reg << " set to " << getRegisterContent( reg );

  if ( reg == '0' || reg == '1' || reg == '-' ) {
    m_defaultRegister = reg;
    kDebug( 13070 ) << "Register " << '"' << " set to point to \"" << reg;
  }
}

void KateViGlobal::addMapping( ViMode mode, const QString &from, const QString &to, KateViModeBase::MappingRecursion recursion )
{
  if ( !from.isEmpty() ) {
    switch ( mode ) {
    case NormalMode:
      m_normalModeMappings[ KateViKeyParser::self()->encodeKeySequence( from ) ]
        = KateViKeyParser::self()->encodeKeySequence( to );
      m_normalModeMappingRecursion [ KateViKeyParser::self()->encodeKeySequence( from ) ] = recursion;
      break;
    case VisualMode:
    case VisualLineMode:
    case VisualBlockMode:
      m_visualModeMappings[ KateViKeyParser::self()->encodeKeySequence( from ) ]
        = KateViKeyParser::self()->encodeKeySequence( to );
      m_visualModeMappingRecursion [ KateViKeyParser::self()->encodeKeySequence( from ) ] = recursion;
      break;
    case InsertMode:
      m_insertModeMappings[ KateViKeyParser::self()->encodeKeySequence( from ) ]
        = KateViKeyParser::self()->encodeKeySequence( to );
      m_insertModeMappingRecursion [ KateViKeyParser::self()->encodeKeySequence( from ) ] = recursion;
      break;
    default:
      kDebug( 13070 ) << "Mapping not supported for given mode";
    }
  }
}

const QString KateViGlobal::getMapping( ViMode mode, const QString &from, bool decode ) const
{
    QString ret;
    switch ( mode ) {
    case NormalMode:
      ret = m_normalModeMappings.value( from );
      break;
    case VisualMode:
    case VisualLineMode:
    case VisualBlockMode:
      ret = m_visualModeMappings.value( from );
      break;
    case InsertMode:
      ret = m_insertModeMappings.value( from );
      break;
    default:
      kDebug( 13070 ) << "Mapping not supported for given mode";
    }

    if ( decode ) {
      return KateViKeyParser::self()->decodeKeySequence( ret );
    }
    return ret;
}

const QStringList KateViGlobal::getMappings( ViMode mode, bool decode ) const
{
  QStringList l;
  QHash<QString, QString>::const_iterator i;
  switch (mode ) {
  case NormalMode:
    for (i = m_normalModeMappings.constBegin(); i != m_normalModeMappings.constEnd(); ++i) {
      if ( decode ) {
        l << KateViKeyParser::self()->decodeKeySequence( i.key() );
      } else {
        l << i.key();
      }
    }
    break;
  case VisualMode:
  case VisualLineMode:
  case VisualBlockMode:
    for (i = m_visualModeMappings.constBegin(); i != m_visualModeMappings.constEnd(); ++i) {
      if ( decode ) {
        l << KateViKeyParser::self()->decodeKeySequence( i.key() );
      } else {
        l << i.key();
      }
    }
    break;
  case InsertMode:
    for (i = m_insertModeMappings.constBegin(); i != m_insertModeMappings.constEnd(); ++i) {
      if ( decode ) {
        l << KateViKeyParser::self()->decodeKeySequence( i.key() );
      } else {
        l << i.key();
      }
    }
    break;
  default:
    kDebug( 13070 ) << "Mapping not supported for given mode";
  }

  return l;
}

bool KateViGlobal::isMappingRecursive(ViMode mode, const QString& from) const
{
    switch ( mode ) {
    case NormalMode:
      return (m_normalModeMappingRecursion.value( from ) == KateViModeBase::Recursive);
    case VisualMode:
    case VisualLineMode:
    case VisualBlockMode:
      return (m_visualModeMappingRecursion.value( from ) == KateViModeBase::Recursive);
    case InsertMode:
      return (m_insertModeMappingRecursion.value( from ) == KateViModeBase::Recursive);
    default:
      kDebug( 13070 ) << "Mapping not supported for given mode";
      return false;
    }
}

void KateViGlobal::clearMappings( ViMode mode )
{
  switch (mode ) {
  case NormalMode:
    m_normalModeMappings.clear();
    break;
  case VisualMode:
  case VisualLineMode:
  case VisualBlockMode:
    m_visualModeMappings.clear();
    break;
  case InsertMode:
    m_insertModeMappings.clear();
    break;
  default:
    kDebug( 13070 ) << "Mapping not supported for given mode";
  }
}

void KateViGlobal::clearSearchHistory()
{
  m_searchHistory.clear();
}

QStringList KateViGlobal::searchHistory()
{
  return m_searchHistory.items();
}

void KateViGlobal::appendSearchHistoryItem(const QString& searchHistoryItem)
{
  m_searchHistory.appendItem(searchHistoryItem);
}

QStringList KateViGlobal::commandHistory()
{
  return m_commandHistory.items();
}

void KateViGlobal::clearCommandHistory()
{
  m_commandHistory.clear();
}

void KateViGlobal::appendCommandHistoryItem(const QString& commandHistoryItem)
{
  m_commandHistory.appendItem(commandHistoryItem);
}

QStringList KateViGlobal::replaceHistory()
{
  return m_replaceHistory.items();
}

void KateViGlobal::appendReplaceHistoryItem(const QString& replaceHistoryItem)
{
  m_replaceHistory.appendItem(replaceHistoryItem);
}

void KateViGlobal::clearReplaceHistory()
{
  m_replaceHistory.clear();
}