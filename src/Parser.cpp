/*----------------------------------------------------------------------
Compiler Generator Coco/R,
Copyright (c) 1990, 2004 Hanspeter Moessenboeck, University of Linz
extended by M. Loeberbauer & A. Woess, Univ. of Linz
ported to C++ by Csaba Balazs, University of Szeged
with improvements by Pat Terry, Rhodes University

This program is free software; you can redistribute it and/or modify it 
under the terms of the GNU General Public License as published by the 
Free Software Foundation; either version 2, or (at your option) any 
later version.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License 
for more details.

You should have received a copy of the GNU General Public License along 
with this program; if not, write to the Free Software Foundation, Inc., 
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

As an exception, it is allowed to write an extension of Coco/R that is
used as a plugin in non-free software.

If not otherwise stated, any source code generated by Coco/R (other than 
Coco/R itself) does not fall under the GNU General Public License.
-----------------------------------------------------------------------*/


#include <wchar.h>
#include "Parser.h"
#include "Scanner.h"


namespace Coco {


#ifdef PARSER_WITH_AST

void Parser::AstAddTerminal() {
        SynTree *st_t = new SynTree( t->Clone() );
        ast_stack.Top()->children.Add(st_t);
}

bool Parser::AstAddNonTerminal(eNonTerminals kind, const char *nt_name, int line) {
        Token *ntTok = new Token();
        ntTok->kind = kind;
        ntTok->line = line;
        ntTok->val = coco_string_create(nt_name);
        SynTree *st = new SynTree( ntTok );
        ast_stack.Top()->children.Add(st);
        ast_stack.Add(st);
        return true;
}

void Parser::AstPopNonTerminal() {
        ast_stack.Pop();
}

#endif

void Parser::SynErr(int n) {
	if (errDist >= minErrDist) errors.SynErr(la->line, la->col, n);
	errDist = 0;
}

void Parser::SemErr(const wchar_t* msg) {
	if (errDist >= minErrDist) errors.Error(t->line, t->col, msg);
	errDist = 0;
}

void Parser::Get() {
	for (;;) {
		t = la;
		la = scanner->Scan();
		if (la->kind <= maxT) { ++errDist; break; }
		if (la->kind == _ddtSym) {
				tab->SetDDT(la->val); 
		}
		if (la->kind == _optionSym) {
				tab->SetOption(la->val); 
		}

		if (dummyToken != t) {
			dummyToken->kind = t->kind;
			dummyToken->pos = t->pos;
			dummyToken->col = t->col;
			dummyToken->line = t->line;
			dummyToken->next = NULL;
			coco_string_delete(dummyToken->val);
			dummyToken->val = coco_string_create(t->val);
			t = dummyToken;
		}
		la = t;
	}
}

void Parser::Expect(int n) {
	if (la->kind==n) Get(); else { SynErr(n); }
}

void Parser::ExpectWeak(int n, int follow) {
	if (la->kind == n) Get();
	else {
		SynErr(n);
		while (!StartOf(follow)) Get();
	}
}

bool Parser::WeakSeparator(int n, int syFol, int repFol) {
	if (la->kind == n) {Get(); return true;}
	else if (StartOf(repFol)) {return false;}
	else {
		SynErr(n);
		while (!(StartOf(syFol) || StartOf(repFol) || StartOf(0))) {
			Get();
		}
		return StartOf(syFol);
	}
}

void Parser::Coco() {
		Symbol *sym; Graph *g, *g1, *g2; wchar_t* gramName = NULL; CharSet *s; 
#ifdef PARSER_WITH_AST
		Token *ntTok = new Token(); ntTok->kind = eNonTerminals::_Coco; ntTok->line = 0; ntTok->val = coco_string_create("Coco");ast_root = new SynTree( ntTok ); ast_stack.Clear(); ast_stack.Add(ast_root);
#endif
		int beg = la->pos; int line = la->line; 
		while (StartOf(1 /* any  */)) {
			Get();
		}
		if (la->pos != beg) {
		 pgen->usingPos = new Position(beg, t->pos + coco_string_length(t->val), 0, line);
		}
		
		Expect(6 /* "COMPILER" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
		genScanner = true; 
		tab->ignored = new CharSet(); 
		Expect(_ident);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
		gramName = coco_string_create(t->val);
		beg = la->pos;
		line = la->line;
		
		while (StartOf(2 /* any  */)) {
			Get();
		}
		tab->semDeclPos = new Position(beg, la->pos, 0, line); 
		if (la->kind == 7 /* "IGNORECASE" */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			dfa->ignoreCase = true; 
		}
		if (la->kind == 8 /* "TERMINALS" */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			while (la->kind == _ident) {
				Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
				sym = tab->FindSym(t->val);
				       if (sym != NULL) SemErr(_SC("name declared twice"));
				       else {
				        sym = tab->NewSym(Node::t, t->val, t->line, t->col);
				        sym->tokenKind = Symbol::fixedToken;
				}
			}
		}
		if (la->kind == 9 /* "CHARACTERS" */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			while (la->kind == _ident) {
				SetDecl();
			}
		}
		if (la->kind == 10 /* "TOKENS" */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			while (la->kind == _ident || la->kind == _string || la->kind == _char) {
				TokenDecl(Node::t);
			}
		}
		if (la->kind == 11 /* "PRAGMAS" */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			while (la->kind == _ident || la->kind == _string || la->kind == _char) {
				TokenDecl(Node::pr);
			}
		}
		while (la->kind == 12 /* "COMMENTS" */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			bool nested = false; 
			Expect(13 /* "FROM" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			TokenExpr(g1);
			Expect(14 /* "TO" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			TokenExpr(g2);
			if (la->kind == 15 /* "NESTED" */) {
				Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
				nested = true; 
			}
			dfa->NewComment(g1->l, g2->l, nested); delete g1; delete g2; 
		}
		while (la->kind == 16 /* "IGNORE" */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			Set(s);
			tab->ignored->Or(s); delete s; 
		}
		while (!(la->kind == _EOF || la->kind == 17 /* "PRODUCTIONS" */)) {SynErr(43); Get();}
		Expect(17 /* "PRODUCTIONS" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
		if (genScanner) dfa->MakeDeterministic();
		tab->DeleteNodes();
		
		while (la->kind == _ident) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			sym = tab->FindSym(t->val);
			bool undef = (sym == NULL);
			if (undef) sym = tab->NewSym(Node::nt, t->val, t->line, t->col);
			else {
			 if (sym->typ == Node::nt) {
			   if (sym->graph != NULL) SemErr(_SC("name declared twice"));
			 } else SemErr(_SC("this symbol kind not allowed on left side of production"));
			 sym->line = t->line;
			}
			bool noAttrs = (sym->attrPos == NULL);
			sym->attrPos = NULL;
			
			if (la->kind == 25 /* "<" */ || la->kind == 27 /* "<." */) {
				AttrDecl(sym);
			}
			if (!undef)
			 if (noAttrs != (sym->attrPos == NULL))
			   SemErr(_SC("attribute mismatch between declaration and use of this symbol"));
			
			if (la->kind == 40 /* "(." */) {
				SemText(sym->semPos);
			}
			ExpectWeak(18 /* "=" */, 3);
			Expression(g);
			sym->graph = g->l;
			tab->Finish(g);
			delete g;
			
			ExpectWeak(19 /* "." */, 4);
		}
		Expect(20 /* "END" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
		Expect(_ident);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
		if (!coco_string_equal(gramName, t->val))
		 SemErr(_SC("name does not match grammar name"));
		tab->gramSy = tab->FindSym(gramName);
		coco_string_delete(gramName);
		if (tab->gramSy == NULL)
		 SemErr(_SC("missing production for grammar name"));
		else {
		 sym = tab->gramSy;
		 if (sym->attrPos != NULL)
		   SemErr(_SC("grammar symbol must not have attributes"));
		}
		tab->noSym = tab->NewSym(Node::t, _SC("???"), 0, 0); // noSym gets highest number
		tab->SetupAnys();
		tab->RenumberPragmas();
		if (tab->ddt[2]) tab->PrintNodes();
		if (errors.count == 0) {
		 wprintf(_SC("checking\n"));
		 tab->CompSymbolSets();
		 if (tab->ddt[7]) tab->XRef();
		 bool doGenCode = false;
		 if(ignoreGammarErrors) {
		   doGenCode = true;
		   tab->GrammarCheckAll();
		 }
		 else doGenCode = tab->GrammarOk();
		 if (doGenCode) {
		   wprintf(_SC("parser"));
		   pgen->WriteParser();
		   if (genScanner) {
		     wprintf(_SC(" + scanner"));
		     dfa->WriteScanner();
		     if (tab->ddt[0]) dfa->PrintStates();
		   }
		   wprintf(_SC(" generated\n"));
		   if (tab->ddt[8]) pgen->WriteStatistics();
		 }
		}
		if (tab->ddt[6]) tab->PrintSymbolTable();
		
		Expect(19 /* "." */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
#ifdef PARSER_WITH_AST
		AstPopNonTerminal();
#endif
}

void Parser::SetDecl() {
		CharSet *s; 
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_SetDecl, "SetDecl", la->line);
#endif
		Expect(_ident);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
		wchar_t *name = coco_string_create(t->val);
		CharClass *c = tab->FindCharClass(name);
		if (c != NULL) SemErr(_SC("name declared twice"));
		
		Expect(18 /* "=" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
		Set(s);
		if (s->Elements() == 0) SemErr(_SC("character set must not be empty"));
		tab->NewCharClass(name, s);
		coco_string_delete(name);
		
		Expect(19 /* "." */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}

void Parser::TokenDecl(int typ) {
		wchar_t* name = NULL; int kind; Symbol *sym; Graph *g; 
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_TokenDecl, "TokenDecl", la->line);
#endif
		Sym(name, kind);
		sym = tab->FindSym(name);
		if (sym != NULL) SemErr(_SC("name declared twice"));
		else {
		 sym = tab->NewSym(typ, name, t->line, t->col);
		 sym->tokenKind = Symbol::fixedToken;
		}
		coco_string_delete(name);
		coco_string_delete(tokenString);
		
		while (!(StartOf(5 /* sync */))) {SynErr(44); Get();}
		if (la->kind == 18 /* "=" */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			TokenExpr(g);
			Expect(19 /* "." */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			if (kind == str) SemErr(_SC("a literal must not be declared with a structure"));
			tab->Finish(g);
			if (tokenString == NULL || coco_string_equal(tokenString, noString))
			 dfa->ConvertToStates(g->l, sym);
			else { // TokenExpr is a single string
			 if (tab->literals[tokenString] != NULL)
			   SemErr(_SC("token string declared twice"));
			 tab->literals.Set(tokenString, sym);
			 dfa->MatchLiteral(tokenString, sym);
			}
			delete g;
			
		} else if (StartOf(6 /* sem  */)) {
			if (kind == id) genScanner = false;
			else dfa->MatchLiteral(sym->name, sym);
			
		} else SynErr(45);
		if (la->kind == 40 /* "(." */) {
			SemText(sym->semPos);
			if (typ == Node::t) errors.Warning(_SC("Warning semantic action on token declarations require a custom Scanner")); 
		}
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}

void Parser::TokenExpr(Graph* &g) {
		Graph *g2; 
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_TokenExpr, "TokenExpr", la->line);
#endif
		TokenTerm(g);
		bool first = true; 
		while (WeakSeparator(29 /* "|" */,8,7) ) {
			TokenTerm(g2);
			if (first) { tab->MakeFirstAlt(g); first = false; }
			tab->MakeAlternative(g, g2); delete g2;
			
		}
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}

void Parser::Set(CharSet* &s) {
		CharSet *s2; 
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_Set, "Set", la->line);
#endif
		SimSet(s);
		while (la->kind == 21 /* "+" */ || la->kind == 22 /* "-" */) {
			if (la->kind == 21 /* "+" */) {
				Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
				SimSet(s2);
				s->Or(s2); delete s2; 
			} else {
				Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
				SimSet(s2);
				s->Subtract(s2); delete s2; 
			}
		}
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}

void Parser::AttrDecl(Symbol *sym) {
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_AttrDecl, "AttrDecl", la->line);
#endif
		if (la->kind == 25 /* "<" */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			int beg = la->pos; int col = la->col; int line = la->line; 
			while (StartOf(9 /* alt  */)) {
				if (StartOf(10 /* any  */)) {
					Get();
				} else {
					Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
					SemErr(_SC("bad string in attributes")); 
				}
			}
			Expect(26 /* ">" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			if (t->pos > beg)
			 sym->attrPos = new Position(beg, t->pos, col, line); 
		} else if (la->kind == 27 /* "<." */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			int beg = la->pos; int col = la->col; int line = la->line; 
			while (StartOf(11 /* alt  */)) {
				if (StartOf(12 /* any  */)) {
					Get();
				} else {
					Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
					SemErr(_SC("bad string in attributes")); 
				}
			}
			Expect(28 /* ".>" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			if (t->pos > beg)
			 sym->attrPos = new Position(beg, t->pos, col, line); 
		} else SynErr(46);
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}

void Parser::SemText(Position* &pos) {
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_SemText, "SemText", la->line);
#endif
		Expect(40 /* "(." */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
		int beg = la->pos; int col = la->col; int line = t->line; 
		while (StartOf(13 /* alt  */)) {
			if (StartOf(14 /* any  */)) {
				Get();
			} else if (la->kind == _badString) {
				Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
				SemErr(_SC("bad string in semantic action")); 
			} else {
				Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
				SemErr(_SC("missing end of previous semantic action")); 
			}
		}
		Expect(41 /* ".)" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
		pos = new Position(beg, t->pos, col, line); 
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}

void Parser::Expression(Graph* &g) {
		Graph *g2; 
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_Expression, "Expression", la->line);
#endif
		Term(g);
		bool first = true; 
		while (WeakSeparator(29 /* "|" */,16,15) ) {
			Term(g2);
			if (first) { tab->MakeFirstAlt(g); first = false; }
			tab->MakeAlternative(g, g2); delete g2;
			
		}
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}

void Parser::SimSet(CharSet* &s) {
		int n1, n2; 
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_SimSet, "SimSet", la->line);
#endif
		s = new CharSet(); 
		if (la->kind == _ident) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			CharClass *c = tab->FindCharClass(t->val);
			if (c == NULL) SemErr(_SC("undefined name")); else s->Or(c->set);
			
		} else if (la->kind == _string) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			wchar_t *subName2 = coco_string_create(t->val, 1, coco_string_length(t->val)-2);
			wchar_t *name = tab->Unescape(subName2);
			coco_string_delete(subName2);
			wchar_t ch;
			int len = coco_string_length(name);
			for(int i=0; i < len; i++) {
			 ch = name[i];
			 if (dfa->ignoreCase) {
			   if ((_SC('A') <= ch) && (ch <= _SC('Z'))) ch = ch - (_SC('A') - _SC('a')); // ch.ToLower()
			 }
			 s->Set(ch);
			}
			coco_string_delete(name);
			
		} else if (la->kind == _char) {
			Char(n1);
			s->Set(n1); 
			if (la->kind == 23 /* ".." */) {
				Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
				Char(n2);
				for (int i = n1; i <= n2; i++) s->Set(i); 
			}
		} else if (la->kind == 24 /* "ANY" */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			delete s; s = new CharSet(); s->Fill(); 
		} else SynErr(47);
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}

void Parser::Char(int &n) {
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_Char, "Char", la->line);
#endif
		Expect(_char);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
		n = 0;
		wchar_t* subName = coco_string_create(t->val, 1, coco_string_length(t->val)-2);
		wchar_t* name = tab->Unescape(subName);
		coco_string_delete(subName);
		
		// "<= 1" instead of "== 1" to allow the escape sequence '\0' in c++
		if (coco_string_length(name) <= 1) n = name[0];
		else SemErr(_SC("unacceptable character value"));
		coco_string_delete(name);
		if (dfa->ignoreCase && (((wchar_t) n) >= 'A') && (((wchar_t) n) <= 'Z')) n += 32;
		
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}

void Parser::Sym(wchar_t* &name, int &kind) {
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_Sym, "Sym", la->line);
#endif
		name = coco_string_create(_SC("???")); kind = id; 
		if (la->kind == _ident) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			kind = id; coco_string_delete(name); name = coco_string_create(t->val); 
		} else if (la->kind == _string || la->kind == _char) {
			if (la->kind == _string) {
				Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
				coco_string_delete(name); name = coco_string_create(t->val); 
			} else {
				Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
				wchar_t *subName = coco_string_create(t->val, 1, coco_string_length(t->val)-2);
				coco_string_delete(name);
				name = coco_string_create_append(_SC("\""), subName);
				coco_string_delete(subName);
				coco_string_merge(name, _SC("\""));
				
			}
			kind = str;
			if (dfa->ignoreCase) {
			wchar_t *oldName = name;
			name = coco_string_create_lower(name);
			coco_string_delete(oldName);
			}
			if (coco_string_indexof(name, ' ') >= 0)
			 SemErr(_SC("literal tokens must not contain blanks")); 
		} else SynErr(48);
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}

void Parser::Term(Graph* &g) {
		Graph *g2; Node *rslv = NULL; g = NULL; 
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_Term, "Term", la->line);
#endif
		if (StartOf(17 /* opt  */)) {
			if (la->kind == 38 /* "IF" */) {
				rslv = tab->NewNode(Node::rslv, (Symbol*)NULL, la->line, la->col); 
				Resolver(rslv->pos);
				g = new Graph(rslv); 
			}
			Factor(g2);
			if (rslv != NULL) {tab->MakeSequence(g, g2); delete g2;}
			else g = g2; 
			while (StartOf(18 /* nt   */)) {
				Factor(g2);
				tab->MakeSequence(g, g2); delete g2; 
			}
		} else if (StartOf(19 /* sem  */)) {
			g = new Graph(tab->NewNode(Node::eps, (Symbol*)NULL, 0, 0)); 
		} else SynErr(49);
		if (g == NULL) // invalid start of Term
		g = new Graph(tab->NewNode(Node::eps, (Symbol*)NULL, 0, 0)); 
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}

void Parser::Resolver(Position* &pos) {
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_Resolver, "Resolver", la->line);
#endif
		Expect(38 /* "IF" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
		Expect(31 /* "(" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
		int beg = la->pos; int col = la->col; int line = la->line; 
		Condition();
		pos = new Position(beg, t->pos, col, line); 
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}

void Parser::Factor(Graph* &g) {
		wchar_t* name = NULL; int kind; Position *pos; bool weak = false; 
		 g = NULL;
		
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_Factor, "Factor", la->line);
#endif
		switch (la->kind) {
		case _ident: case _string: case _char: case 30 /* "WEAK" */: {
			if (la->kind == 30 /* "WEAK" */) {
				Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
				weak = true; 
			}
			Sym(name, kind);
			Symbol *sym = tab->FindSym(name);
			 if (sym == NULL && kind == str)
			   sym = (Symbol*)tab->literals[name];
			 bool undef = (sym == NULL);
			 if (undef) {
			   if (kind == id)
			     sym = tab->NewSym(Node::nt, name, 0, 0);  // forward nt
			   else if (genScanner) { 
			     sym = tab->NewSym(Node::t, name, t->line, t->col);
			     dfa->MatchLiteral(sym->name, sym);
			   } else {  // undefined string in production
			     SemErr(_SC("undefined string in production"));
			     sym = tab->eofSy;  // dummy
			   }
			 }
			 coco_string_delete(name);
			 int typ = sym->typ;
			 if (typ != Node::t && typ != Node::nt)
			   SemErr(_SC("this symbol kind is not allowed in a production"));
			 if (weak) {
			   if (typ == Node::t) typ = Node::wt;
			   else SemErr(_SC("only terminals may be weak"));
			 }
			 Node *p = tab->NewNode(typ, sym, t->line, t->col);
			 g = new Graph(p);
			
			if (la->kind == 25 /* "<" */ || la->kind == 27 /* "<." */) {
				Attribs(p);
				if (kind != id) SemErr(_SC("a literal must not have attributes")); 
			}
			if (undef)
			 sym->attrPos = p->pos;  // dummy
			else if ((p->pos == NULL) != (sym->attrPos == NULL))
			 SemErr(_SC("attribute mismatch between declaration and use of this symbol"));
			
			break;
		}
		case 31 /* "(" */: {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			Expression(g);
			Expect(32 /* ")" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			break;
		}
		case 33 /* "[" */: {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			Expression(g);
			Expect(34 /* "]" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			tab->MakeOption(g); 
			break;
		}
		case 35 /* "{" */: {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			Expression(g);
			Expect(36 /* "}" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			tab->MakeIteration(g); 
			break;
		}
		case 40 /* "(." */: {
			SemText(pos);
			Node *p = tab->NewNode(Node::sem, (Symbol*)NULL, 0, 0);
			   p->pos = pos;
			   g = new Graph(p);
			 
			break;
		}
		case 24 /* "ANY" */: {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			Node *p = tab->NewNode(Node::any, (Symbol*)NULL, 0, 0);  // p.set is set in tab->SetupAnys
			g = new Graph(p);
			
			break;
		}
		case 37 /* "SYNC" */: {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			Node *p = tab->NewNode(Node::sync, (Symbol*)NULL, 0, 0);
			g = new Graph(p);
			
			break;
		}
		default: SynErr(50); break;
		}
		if (g == NULL) // invalid start of Factor
		 g = new Graph(tab->NewNode(Node::eps, (Symbol*)NULL, 0, 0));
		
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}

void Parser::Attribs(Node *p) {
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_Attribs, "Attribs", la->line);
#endif
		if (la->kind == 25 /* "<" */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			int beg = la->pos; int col = la->col; int line = la->line; 
			while (StartOf(9 /* alt  */)) {
				if (StartOf(10 /* any  */)) {
					Get();
				} else {
					Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
					SemErr(_SC("bad string in attributes")); 
				}
			}
			Expect(26 /* ">" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			if (t->pos > beg) p->pos = new Position(beg, t->pos, col, line); 
		} else if (la->kind == 27 /* "<." */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			int beg = la->pos; int col = la->col; int line = la->line; 
			while (StartOf(11 /* alt  */)) {
				if (StartOf(12 /* any  */)) {
					Get();
				} else {
					Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
					SemErr(_SC("bad string in attributes")); 
				}
			}
			Expect(28 /* ".>" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			if (t->pos > beg) p->pos = new Position(beg, t->pos, col, line); 
		} else SynErr(51);
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}

void Parser::Condition() {
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_Condition, "Condition", la->line);
#endif
		while (StartOf(20 /* alt  */)) {
			if (la->kind == 31 /* "(" */) {
				Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
				Condition();
			} else {
				Get();
			}
		}
		Expect(32 /* ")" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}

void Parser::TokenTerm(Graph* &g) {
		Graph *g2; 
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_TokenTerm, "TokenTerm", la->line);
#endif
		TokenFactor(g);
		while (StartOf(8 /* nt   */)) {
			TokenFactor(g2);
			tab->MakeSequence(g, g2); delete g2; 
		}
		if (la->kind == 39 /* "CONTEXT" */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			Expect(31 /* "(" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			TokenExpr(g2);
			tab->SetContextTrans(g2->l); dfa->hasCtxMoves = true;
			   tab->MakeSequence(g, g2); delete g2; 
			Expect(32 /* ")" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
		}
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}

void Parser::TokenFactor(Graph* &g) {
		wchar_t* name = NULL; int kind; 
#ifdef PARSER_WITH_AST
		bool ntAdded = AstAddNonTerminal(eNonTerminals::_TokenFactor, "TokenFactor", la->line);
#endif
		g = NULL; 
		if (la->kind == _ident || la->kind == _string || la->kind == _char) {
			Sym(name, kind);
			if (kind == id) {
			   CharClass *c = tab->FindCharClass(name);
			   if (c == NULL) {
			     SemErr(_SC("undefined name"));
			     c = tab->NewCharClass(name, new CharSet());
			   }
			   Node *p = tab->NewNode(Node::clas, (Symbol*)NULL, 0, 0); p->val = c->n;
			   g = new Graph(p);
			   coco_string_delete(tokenString); tokenString = coco_string_create(noString);
			 } else { // str
			   g = tab->StrToGraph(name);
			   if (tokenString == NULL) tokenString = coco_string_create(name);
			   else {
			     coco_string_delete(tokenString);
			     tokenString = coco_string_create(noString);
			   }
			 }
			 coco_string_delete(name);
			
		} else if (la->kind == 31 /* "(" */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			TokenExpr(g);
			Expect(32 /* ")" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
		} else if (la->kind == 33 /* "[" */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			TokenExpr(g);
			Expect(34 /* "]" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			tab->MakeOption(g); coco_string_delete(tokenString); tokenString = coco_string_create(noString); 
		} else if (la->kind == 35 /* "{" */) {
			Get();
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			TokenExpr(g);
			Expect(36 /* "}" */);
#ifdef PARSER_WITH_AST
	AstAddTerminal();
#endif
			tab->MakeIteration(g); coco_string_delete(tokenString); tokenString = coco_string_create(noString); 
		} else SynErr(52);
		if (g == NULL) // invalid start of TokenFactor
		 g = new Graph(tab->NewNode(Node::eps, (Symbol*)NULL, 0, 0)); 
#ifdef PARSER_WITH_AST
		if(ntAdded) AstPopNonTerminal();
#endif
}




// If the user declared a method Init and a mehtod Destroy they should
// be called in the contructur and the destructor respctively.
//
// The following templates are used to recognize if the user declared
// the methods Init and Destroy.

template<typename T>
struct ParserInitExistsRecognizer {
	template<typename U, void (U::*)() = &U::Init>
	struct ExistsIfInitIsDefinedMarker{};

	struct InitIsMissingType {
		char dummy1;
	};

	struct InitExistsType {
		char dummy1; char dummy2;
	};

	// exists always
	template<typename U>
	static InitIsMissingType is_here(...);

	// exist only if ExistsIfInitIsDefinedMarker is defined
	template<typename U>
	static InitExistsType is_here(ExistsIfInitIsDefinedMarker<U>*);

	enum { InitExists = (sizeof(is_here<T>(NULL)) == sizeof(InitExistsType)) };
};

template<typename T>
struct ParserDestroyExistsRecognizer {
	template<typename U, void (U::*)() = &U::Destroy>
	struct ExistsIfDestroyIsDefinedMarker{};

	struct DestroyIsMissingType {
		char dummy1;
	};

	struct DestroyExistsType {
		char dummy1; char dummy2;
	};

	// exists always
	template<typename U>
	static DestroyIsMissingType is_here(...);

	// exist only if ExistsIfDestroyIsDefinedMarker is defined
	template<typename U>
	static DestroyExistsType is_here(ExistsIfDestroyIsDefinedMarker<U>*);

	enum { DestroyExists = (sizeof(is_here<T>(NULL)) == sizeof(DestroyExistsType)) };
};

// The folloing templates are used to call the Init and Destroy methods if they exist.

// Generic case of the ParserInitCaller, gets used if the Init method is missing
template<typename T, bool = ParserInitExistsRecognizer<T>::InitExists>
struct ParserInitCaller {
	static void CallInit(T *t) {
		// nothing to do
	}
};

// True case of the ParserInitCaller, gets used if the Init method exists
template<typename T>
struct ParserInitCaller<T, true> {
	static void CallInit(T *t) {
		t->Init();
	}
};

// Generic case of the ParserDestroyCaller, gets used if the Destroy method is missing
template<typename T, bool = ParserDestroyExistsRecognizer<T>::DestroyExists>
struct ParserDestroyCaller {
	static void CallDestroy(T *t) {
		// nothing to do
	}
};

// True case of the ParserDestroyCaller, gets used if the Destroy method exists
template<typename T>
struct ParserDestroyCaller<T, true> {
	static void CallDestroy(T *t) {
		t->Destroy();
	}
};

void Parser::Parse() {
	t = NULL;
	la = dummyToken = new Token();
	la->val = coco_string_create(_SC("Dummy Token"));
	Get();
	Coco();
	Expect(0);
}

Parser::Parser(Scanner *scanner) {
	maxT = 42;

	ParserInitCaller<Parser>::CallInit(this);
	dummyToken = NULL;
	t = la = NULL;
	minErrDist = 2;
	errDist = minErrDist;
	this->scanner = scanner;
}

bool Parser::StartOf(int s) {
	const bool T = true;
	const bool x = false;

	static bool set[21][44] = {
		{T,T,x,T, x,T,x,x, x,x,x,T, T,x,x,x, T,T,T,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, T,x,x,x},
		{x,T,T,T, T,T,x,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,x},
		{x,T,T,T, T,T,T,x, x,x,x,x, x,T,T,T, x,x,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,x},
		{T,T,x,T, x,T,x,x, x,x,x,T, T,x,x,x, T,T,T,T, x,x,x,x, T,x,x,x, x,T,T,T, x,T,x,T, x,T,T,x, T,x,x,x},
		{T,T,x,T, x,T,x,x, x,x,x,T, T,x,x,x, T,T,T,x, T,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, T,x,x,x},
		{T,T,x,T, x,T,x,x, x,x,x,T, T,x,x,x, T,T,T,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, T,x,x,x},
		{x,T,x,T, x,T,x,x, x,x,x,T, T,x,x,x, T,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, T,x,x,x},
		{x,x,x,x, x,x,x,x, x,x,x,x, T,x,T,T, T,T,x,T, x,x,x,x, x,x,x,x, x,x,x,x, T,x,T,x, T,x,x,x, x,x,x,x},
		{x,T,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, x,T,x,T, x,x,x,x, x,x,x,x},
		{x,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,x,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,x},
		{x,T,T,T, x,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,x,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,x},
		{x,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, x,T,T,T, T,T,T,T, T,T,T,T, T,T,T,x},
		{x,T,T,T, x,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, x,T,T,T, T,T,T,T, T,T,T,T, T,T,T,x},
		{x,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,x,T,x},
		{x,T,T,T, x,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, x,x,T,x},
		{x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, x,x,x,x, x,x,x,x, x,x,x,x, T,x,T,x, T,x,x,x, x,x,x,x},
		{x,T,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,T, x,x,x,x, T,x,x,x, x,T,T,T, T,T,T,T, T,T,T,x, T,x,x,x},
		{x,T,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, T,x,x,x, x,x,T,T, x,T,x,T, x,T,T,x, T,x,x,x},
		{x,T,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, T,x,x,x, x,x,T,T, x,T,x,T, x,T,x,x, T,x,x,x},
		{x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, x,x,x,x, x,x,x,x, x,T,x,x, T,x,T,x, T,x,x,x, x,x,x,x},
		{x,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, T,T,T,T, x,T,T,T, T,T,T,T, T,T,T,x}
	};



	return set[s][la->kind];
}

Parser::~Parser() {
	ParserDestroyCaller<Parser>::CallDestroy(this);
	delete dummyToken;
#ifdef PARSER_WITH_AST
        delete ast_root;
#endif

#ifdef COCO_FRAME_PARSER
        coco_string_delete(noString);
        coco_string_delete(tokenString);
#endif
}

Errors::Errors() {
	count = 0;
}

void Errors::SynErr(int line, int col, int n) {
	const wchar_t* s;
	const size_t format_size = 20;
	wchar_t format[format_size];
	switch (n) {
			case 0: s = _SC("EOF expected"); break;
			case 1: s = _SC("ident expected"); break;
			case 2: s = _SC("number expected"); break;
			case 3: s = _SC("string expected"); break;
			case 4: s = _SC("badString expected"); break;
			case 5: s = _SC("char expected"); break;
			case 6: s = _SC("\"COMPILER\" expected"); break;
			case 7: s = _SC("\"IGNORECASE\" expected"); break;
			case 8: s = _SC("\"TERMINALS\" expected"); break;
			case 9: s = _SC("\"CHARACTERS\" expected"); break;
			case 10: s = _SC("\"TOKENS\" expected"); break;
			case 11: s = _SC("\"PRAGMAS\" expected"); break;
			case 12: s = _SC("\"COMMENTS\" expected"); break;
			case 13: s = _SC("\"FROM\" expected"); break;
			case 14: s = _SC("\"TO\" expected"); break;
			case 15: s = _SC("\"NESTED\" expected"); break;
			case 16: s = _SC("\"IGNORE\" expected"); break;
			case 17: s = _SC("\"PRODUCTIONS\" expected"); break;
			case 18: s = _SC("\"=\" expected"); break;
			case 19: s = _SC("\".\" expected"); break;
			case 20: s = _SC("\"END\" expected"); break;
			case 21: s = _SC("\"+\" expected"); break;
			case 22: s = _SC("\"-\" expected"); break;
			case 23: s = _SC("\"..\" expected"); break;
			case 24: s = _SC("\"ANY\" expected"); break;
			case 25: s = _SC("\"<\" expected"); break;
			case 26: s = _SC("\">\" expected"); break;
			case 27: s = _SC("\"<.\" expected"); break;
			case 28: s = _SC("\".>\" expected"); break;
			case 29: s = _SC("\"|\" expected"); break;
			case 30: s = _SC("\"WEAK\" expected"); break;
			case 31: s = _SC("\"(\" expected"); break;
			case 32: s = _SC("\")\" expected"); break;
			case 33: s = _SC("\"[\" expected"); break;
			case 34: s = _SC("\"]\" expected"); break;
			case 35: s = _SC("\"{\" expected"); break;
			case 36: s = _SC("\"}\" expected"); break;
			case 37: s = _SC("\"SYNC\" expected"); break;
			case 38: s = _SC("\"IF\" expected"); break;
			case 39: s = _SC("\"CONTEXT\" expected"); break;
			case 40: s = _SC("\"(.\" expected"); break;
			case 41: s = _SC("\".)\" expected"); break;
			case 42: s = _SC("??? expected"); break;
			case 43: s = _SC("this symbol not expected in Coco"); break;
			case 44: s = _SC("this symbol not expected in TokenDecl"); break;
			case 45: s = _SC("invalid TokenDecl"); break;
			case 46: s = _SC("invalid AttrDecl"); break;
			case 47: s = _SC("invalid SimSet"); break;
			case 48: s = _SC("invalid Sym"); break;
			case 49: s = _SC("invalid Term"); break;
			case 50: s = _SC("invalid Factor"); break;
			case 51: s = _SC("invalid Attribs"); break;
			case 52: s = _SC("invalid TokenFactor"); break;

		default:
		{
			coco_swprintf(format, format_size, _SC("error %d"), n);
			s = format;
		}
		break;
	}
	wprintf(_SC("-- line %d col %d: %ls\n"), line, col, s);
	count++;
}

void Errors::Error(int line, int col, const wchar_t *s) {
	wprintf(_SC("-- line %d col %d: %ls\n"), line, col, s);
	count++;
}

void Errors::Warning(int line, int col, const wchar_t *s) {
	wprintf(_SC("-- line %d col %d: %ls\n"), line, col, s);
}

void Errors::Warning(const wchar_t *s) {
	wprintf(_SC("%ls\n"), s);
}

void Errors::Exception(const wchar_t* s) {
	wprintf(_SC("%ls"), s);
	exit(1);
}

#ifdef PARSER_WITH_AST

static void printIndent(int n) {
    for(int i=0; i < n; ++i) wprintf(_SC(" "));
}

SynTree::~SynTree() {
    //wprintf(_SC("Token %ls : %d : %d : %d : %d\n"), tok->val, tok->kind, tok->line, tok->col, children.Count);
    delete tok;
    for(int i=0; i<children.Count; ++i) delete ((SynTree*)children[i]);
}

void SynTree::dump(int indent, bool isLast) {
        int last_idx = children.Count;
        if(tok->col) {
            printIndent(indent);
            wprintf(_SC("%s\t%d\t%d\t%d\t%ls\n"), ((isLast || (last_idx == 0)) ? "= " : " "), tok->line, tok->col, tok->kind, tok->val);
        }
        else {
            printIndent(indent);
            wprintf(_SC("%d\t%d\t%d\t%ls\n"), children.Count, tok->line, tok->kind, tok->val);
        }
        if(last_idx) {
                for(int idx=0; idx < last_idx; ++idx) ((SynTree*)children[idx])->dump(indent+4, idx == last_idx);
        }
}

void SynTree::dump2(int maxT, int indent, bool isLast) {
        int last_idx = children.Count;
        if(tok->col) {
            printIndent(indent);
            wprintf(_SC("%s\t%d\t%d\t%d\t%ls\n"), ((isLast || (last_idx == 0)) ? "= " : " "), tok->line, tok->col, tok->kind, tok->val);
        }
        else {
            if(last_idx == 1) {
                if(((SynTree*)children[0])->tok->kind < maxT) {
                    printIndent(indent);
                    wprintf(_SC("%d\t%d\t%d\t%ls\n"), children.Count, tok->line, tok->kind, tok->val);
                }
            }
            else {
                printIndent(indent);
                wprintf(_SC("%d\t%d\t%d\t%ls\n"), children.Count, tok->line, tok->kind, tok->val);
            }
        }
        if(last_idx) {
                for(int idx=0; idx < last_idx; ++idx) ((SynTree*)children[idx])->dump2(maxT, indent+4, idx == last_idx);
        }
}

#endif

} // namespace

