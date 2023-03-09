" Vim syntax file
" Language:	Coal
" Maintainer:	Andrew Apted
" First Author:	Andrew Apted
" Last Change:	2009 Jul 9
" Options:


" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

syn case match

" syncing method
syn sync minlines=100

" Comments
syn keyword coalTodo      contained TODO FIXME XXX
syn match   coalComment   "//.*$" contains=coalTodo,@Spell
syn region  coalComment   start="/[*]" end="[*]/" contains=coalTodo,@Spell

" other keywords
syn keyword coalDefine function var constant native module field
syn keyword coalType void float string vector entity
syn keyword coalStatement return while repeat until for if else assert

" Strings
syn match  coalEscape contained "\\[\\abfnrtv\'\"]\|\\\d\{,3}"
syn region coalString  start=+"+ end=+"+ skip=+\\\\\|\\"+ contains=coalSpecial,@Spell

" integer number
syn match coalFloat  "\<\d\+\>"
" floating point number, with dot, optional exponent
syn match coalFloat  "\<\d\+\.\d*\%(e[-+]\=\d\+\)\=\>"
" floating point number, starting with a dot, optional exponent
syn match coalFloat  "\.\d\+\%(e[-+]\=\d\+\)\=\>"
" floating point number, without dot, with exponent
syn match coalFloat  "\<\d\+e[-+]\=\d\+\>"


" Define the default highlighting.
" For version 5.7 and earlier: only when not done already
" For version 5.8 and later: only when an item doesn't have highlighting yet
if version >= 508 || !exists("did_coal_syntax_inits")
  if version < 508
    let did_coal_syntax_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif

  HiLink coalTodo		Todo
  HiLink coalComment		Comment
  HiLink coalString		String
  HiLink coalEscape		SpecialChar
  HiLink coalFloat		Float
  HiLink coalStatement		Statement
  HiLink coalDefine		Function
  HiLink coalType		Function
  HiLink coalBuiltin		Identifier

  delcommand HiLink
endif

let b:current_syntax = "coal"

" vim: et ts=8
