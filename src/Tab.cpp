/*-------------------------------------------------------------------------
Tab -- Symbol Table Management
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
-------------------------------------------------------------------------*/

#include "Tab.h"
#include "Parser.h"
#include "BitArray.h"

namespace Coco {

const char* Tab::nTyp[] =
		{"    ", "t   ", "pr  ", "nt  ", "clas", "chr ", "wt  ", "any ", "eps ",
		 "sync", "sem ", "alt ", "iter", "opt ", "rslv"};

const char* Tab::tKind[] = {"fixedToken", "classToken", "litToken", "classLitToken"};

Tab::Tab(Parser *parser) {
	for (int i=0; i<10; i++) ddt[i] = false;

	dummyNode = NULL;
	dummyName = 'A';

	this->parser = parser;
	trace = parser->trace;
	errors = parser->errors;
	eofSy = NewSym(Node::t, _SC("EOF"), 0, 0);
	dummyNode = NewNode(Node::eps, (Symbol*)NULL, 0, 0);
	checkEOF = true;
	visited = allSyncSets = NULL;
	srcName = srcDir = nsName = frameDir = outDir = NULL;
	genRREBNF = false;
}

Tab::~Tab() {
    for(int i=0; i<classes.Count; ++i) delete classes[i];
    for(int i=0; i<nodes.Count; ++i) delete nodes[i];
    for(int i=0; i<nonterminals.Count; ++i) delete nonterminals[i];
    for(int i=0; i<pragmas.Count; ++i) delete pragmas[i];
    for(int i=0; i<terminals.Count; ++i) delete terminals[i];
    //delete dummyNode;
    //delete eofSy;
    delete ignored;
    delete visited;
    delete semDeclPos;
    //delete allSyncSets; deleted by ParserGen.cpp:499
    coco_string_delete(srcName);
    coco_string_delete(srcDir);
    coco_string_delete(nsName);
    coco_string_delete(frameDir);
    coco_string_delete(outDir);
}


Symbol* Tab::NewSym(int typ, const wchar_t* name, int line, int col) {
	if (coco_string_length(name) == 2 && name[0] == '"') {
		parser->SemErr(_SC("empty token not allowed"));
		name = coco_string_create(_SC("???"));
	}
	Symbol *sym = new Symbol(typ, name, line, col);

	if (typ == Node::t) {
		sym->n = terminals.Count; terminals.Add(sym);
	} else if (typ == Node::pr) {
		pragmas.Add(sym);
	} else if (typ == Node::nt) {
		sym->n = nonterminals.Count; nonterminals.Add(sym);
	}

	return sym;
}

Symbol* Tab::FindSym(const wchar_t* name) {
	Symbol *s;
	int i;
	for (i=0; i<terminals.Count; i++) {
		s = terminals[i];
		if (coco_string_equal(s->name, name)) return s;
	}
	for (i=0; i<nonterminals.Count; i++) {
		s = nonterminals[i];
		if (coco_string_equal(s->name, name)) return s;
	}
	return NULL;
}

int Tab::Num(const Node *p) {
	if (p == NULL) return 0; else return p->n;
}

void Tab::PrintSym(const Symbol *sym) {
	fwprintf(trace, _SC("%3d %-14.14") _SFMT _SC(" %s"), sym->n, sym->name, nTyp[sym->typ]);

	if (sym->attrPos==NULL) fputws(_SC(" false "), trace); else fputws(_SC(" true  "), trace);
	if (sym->typ == Node::nt) {
		fwprintf(trace, _SC("%5d"), Num(sym->graph));
		if (sym->deletable) fputws(_SC(" true  "), trace); else fputws(_SC(" false "), trace);
	} else
		fputws(_SC("            "), trace);

	fwprintf(trace, _SC("%5d %s\n"), sym->line, tKind[sym->tokenKind]);
}

void Tab::PrintSymbolTable() {
	fwprintf(trace, _SC("%s"),
                "Symbol Table:\n"
                "------------\n\n"
                " nr name          typ  hasAt graph  del    line tokenKind\n");

	Symbol *sym;
	int i;
	for (i=0; i<terminals.Count; i++) {
		sym = terminals[i];
		PrintSym(sym);
	}
	for (i=0; i<pragmas.Count; i++) {
		sym = pragmas[i];
		PrintSym(sym);
	}
	for (i=0; i<nonterminals.Count; i++) {
		sym = nonterminals[i];
		PrintSym(sym);
	}


	fwprintf(trace, _SC("%s"),
                "\nLiteral Tokens:\n"
                "--------------\n");

	Iterator *iter = literals.GetIterator();
	while (iter->HasNext()) {
		DictionaryEntry *e = iter->Next();
		fwprintf(trace, _SC("_%") _SFMT _SC(" =  %") _SFMT _SC(".\n"), ((Symbol*) (e->val))->name, e->key);
	}
        delete iter;
	fputws(_SC("\n"), trace);
}

void Tab::PrintSet(const BitArray *s, int indent) {
	int col, len;
	col = indent;
	Symbol *sym;
	for (int i=0; i<terminals.Count; i++) {
		sym = terminals[i];
		if ((*s)[sym->n]) {
			len = coco_string_length(sym->name);
			if (col + len >= 80) {
				fputws(_SC("\n"), trace);
				for (col = 1; col < indent; col++) fputws(_SC(" "), trace);
			}
			fwprintf(trace, _SC("%") _SFMT _SC(" "), sym->name);
			col += len + 1;
		}
	}
	if (col == indent) fputws(_SC("-- empty set --"), trace);
	fputws(_SC("\n"), trace);
}

//---------------------------------------------------------------------
//  Syntax graph management
//---------------------------------------------------------------------

Node* Tab::NewNode(int typ, Symbol *sym, int line, int col) {
	Node* node = new Node(typ, sym, line, col);
	node->n = nodes.Count;
	nodes.Add(node);
	return node;
}


Node* Tab::NewNode(int typ, Node* sub) {
	Node* node = NewNode(typ, (Symbol*)NULL, 0, 0);
	node->sub = sub;
	return node;
}

Node* Tab::NewNode(int typ, int val, int line, int col) {
	Node* node = NewNode(typ, (Symbol*)NULL, line, col);
	node->val = val;
	return node;
}


void Tab::MakeFirstAlt(Graph *g) {
	g->l = NewNode(Node::alt, g->l); g->l->line = g->l->sub->line;
	g->r->up = true;
	g->l->next = g->r;
	g->r = g->l;
}

// The result will be in g1
void Tab::MakeAlternative(Graph *g1, Graph *g2) {
	g2->l = NewNode(Node::alt, g2->l); g2->l->line = g2->l->sub->line;
	g2->l->up = true;
	g2->r->up = true;
	Node *p = g1->l; while (p->down != NULL) p = p->down;
	p->down = g2->l;
	p = g1->r; while (p->next != NULL) p = p->next;
	// append alternative to g1 end list
	p->next = g2->l;
	// append g2 end list to g1 end list
	g2->l->next = g2->r;
}

// The result will be in g1
void Tab::MakeSequence(Graph *g1, Graph *g2) {
	Node *p = g1->r->next; g1->r->next = g2->l; // link head node
	while (p != NULL) {  // link substructure
		Node *q = p->next; p->next = g2->l;
		p = q;
	}
	g1->r = g2->r;
}

void Tab::MakeIteration(Graph *g) {
	g->l = NewNode(Node::iter, g->l);
	g->r->up = true;
	Node *p = g->r;
	g->r = g->l;
	while (p != NULL) {
		Node *q = p->next; p->next = g->l;
		p = q;
	}
}

void Tab::MakeOption(Graph *g) {
	g->l = NewNode(Node::opt, g->l);
	g->r->up = true;
	g->l->next = g->r;
	g->r = g->l;
}

void Tab::Finish(Graph *g) {
	Node *p = g->r;
	while (p != NULL) {
		Node *q = p->next; p->next = NULL;
		p = q;
	}
}

void Tab::DeleteNodes() {
        for(int i=0; i<nodes.Count; ++i) delete nodes[i];
	nodes.Clear();
	dummyNode = NewNode(Node::eps, (Symbol*)NULL, 0, 0);
}

Graph* Tab::StrToGraph(const wchar_t* str) {
	wchar_t *subStr = coco_string_create(str, 1, coco_string_length(str)-2);
	wchar_t *s = Unescape(subStr);
	coco_string_delete(subStr);
	if (coco_string_length(s) == 0) parser->SemErr(_SC("empty token not allowed"));
	Graph *g = new Graph();
	g->r = dummyNode;
	for (int i = 0; i < coco_string_length(s); i++) {
		Node *p = NewNode(Node::chr, (int)s[i], 0, 0);
		g->r->next = p; g->r = p;
	}
	g->l = dummyNode->next; dummyNode->next = NULL;
	coco_string_delete(s);
	return g;
}


void Tab::SetContextTrans(Node *p) { // set transition code in the graph rooted at p
	while (p != NULL) {
		if (p->typ == Node::chr || p->typ == Node::clas) {
			p->code = Node::contextTrans;
		} else if (p->typ == Node::opt || p->typ == Node::iter) {
			SetContextTrans(p->sub);
		} else if (p->typ == Node::alt) {
			SetContextTrans(p->sub); SetContextTrans(p->down);
		}
		if (p->up) break;
		p = p->next;
	}
}

//------------ graph deletability check -----------------

bool Tab::DelGraph(const Node* p) {
	return p == NULL || (DelNode(p) && DelGraph(p->next));
}

bool Tab::DelSubGraph(const Node* p) {
	return p == NULL || (DelNode(p) && (p->up || DelSubGraph(p->next)));
}

bool Tab::DelNode(const Node* p) {
	if (p->typ == Node::nt) {
		return p->sym->deletable;
	}
	else if (p->typ == Node::alt) {
		return DelSubGraph(p->sub) || (p->down != NULL && DelSubGraph(p->down));
	}
	else {
		return p->typ == Node::iter || p->typ == Node::opt || p->typ == Node::sem
				|| p->typ == Node::eps || p->typ == Node::rslv || p->typ == Node::sync;
	}
}

//----------------- graph printing ----------------------

int Tab::Ptr(const Node *p, bool up) {
	if (p == NULL) return 0;
	else if (up) return -(p->n);
	else return p->n;
}

static const size_t wchar_t_10_sz = 10;
typedef wchar_t wchar_t_10[wchar_t_10_sz];

static wchar_t* TabPos(Position *pos, wchar_t_10 &format) {
	if (pos == NULL) {
		coco_swprintf(format, wchar_t_10_sz, _SC("     "));
	} else {
		coco_swprintf(format, wchar_t_10_sz, _SC("%5d"), pos->beg);
	}
	return format;
}

void Tab::PrintNodes() {
	fwprintf(trace, _SC("%s"),
                "Graph nodes:\n"
                "----------------------------------------------------\n"
                "   n type name          next  down   sub   pos  line\n"
                "                               val  code\n"
                "----------------------------------------------------\n");

	Node *p;
        wchar_t_10 format;
	for (int i=0; i<nodes.Count; i++) {
		p = nodes[i];
		fwprintf(trace, _SC("%4d %s "), p->n, (nTyp[p->typ]));
		if (p->sym != NULL) {
			fwprintf(trace, _SC("%-12.12") _SFMT _SC(" "), p->sym->name);
		} else if (p->typ == Node::clas) {
			CharClass *c = classes[p->val];
			fwprintf(trace, _SC("%-12.12") _SFMT _SC(" "), c->name);
		} else fputws(_SC("             "), trace);
		fwprintf(trace, _SC("%5d "), Ptr(p->next, p->up));

		if (p->typ == Node::t || p->typ == Node::nt || p->typ == Node::wt) {
			fwprintf(trace, _SC("             %5") _SFMT, TabPos(p->pos, format));
		} if (p->typ == Node::chr) {
			fwprintf(trace, _SC("%5d %5d       "), p->val, p->code);
		} if (p->typ == Node::clas) {
			fwprintf(trace, _SC("      %5d       "), p->code);
		} if (p->typ == Node::alt || p->typ == Node::iter || p->typ == Node::opt) {
			fwprintf(trace, _SC("%5d %5d       "), Ptr(p->down, false), Ptr(p->sub, false));
		} if (p->typ == Node::sem) {
			fwprintf(trace, _SC("             %5") _SFMT, TabPos(p->pos, format));
		} if (p->typ == Node::eps || p->typ == Node::any || p->typ == Node::sync) {
			fwprintf(trace, _SC("                  "));
		}
		fwprintf(trace, _SC("%5d\n"), p->line);
	}
	fputws(_SC("\n"), trace);
}

//---------------------------------------------------------------------
//  Character class management
//---------------------------------------------------------------------


CharClass* Tab::NewCharClass(const wchar_t* name, CharSet *s) {
	CharClass *c;
	if (coco_string_equal(name, _SC("#"))) {
		wchar_t* temp = coco_string_create_append(name, (wchar_t) dummyName++);
		c = new CharClass(temp, s);
		coco_string_delete(temp);
	} else {
		c = new CharClass(name, s);
	}
	c->n = classes.Count;
	classes.Add(c);
	return c;
}

CharClass* Tab::FindCharClass(const wchar_t* name) {
	CharClass *c;
	for (int i=0; i<classes.Count; i++) {
		c = classes[i];
		if (coco_string_equal(c->name, name)) return c;
	}
	return NULL;
}

CharClass* Tab::FindCharClass(const CharSet *s) {
	CharClass *c;
	for (int i=0; i<classes.Count; i++) {
		c = classes[i];
		if (s->Equals(c->set)) return c;
	}
	return NULL;
}

CharSet* Tab::CharClassSet(int i) {
	return classes[i]->set;
}

//----------- character class printing

wchar_t* TabCh(const int ch, wchar_t_10 &format) {
	if (ch < _SC(' ') || ch >= 127 || ch == _SC('\'') || ch == _SC('\\')) {
		coco_swprintf(format, wchar_t_10_sz, _SC("%d"), ch);
		return format;
	} else {
		coco_swprintf(format, wchar_t_10_sz, _SC("'%") _CHFMT _SC("'"), ch);
		return format;
	}
}

void Tab::WriteCharSet(const CharSet *s) {
        wchar_t_10 fmt1, fmt2;
	for (CharSet::Range *r = s->head; r != NULL; r = r->next) {
		if (r->from < r->to) {
			wchar_t *from = TabCh(r->from, fmt1);
			wchar_t *to = TabCh(r->to, fmt2);
			fwprintf(trace, _SC("%") _SFMT _SC(" .. %") _SFMT _SC(" "), from, to);
		}
		else {
			wchar_t *from = TabCh(r->from, fmt1);
			fwprintf(trace, _SC("%") _SFMT _SC(" "), from);
		}
	}
}

void Tab::WriteCharClasses () {
	CharClass *c;
	for (int i=0; i<classes.Count; i++) {
		c = classes[i];
		fwprintf(trace, _SC("%-10.10") _SFMT _SC(": "), c->name);
		WriteCharSet(c->set);
		fputws(_SC("\n"), trace);
	}
	fputws(_SC("\n"), trace);
}

//---------------------------------------------------------------------
//  Symbol set computations
//---------------------------------------------------------------------

/* Computes the first set for the given Node. */
BitArray* Tab::First0(const Node *p, BitArray *mark) {
	BitArray *fs = new BitArray(terminals.Count);
	while (p != NULL && !((*mark)[p->n])) {
		mark->Set(p->n, true);
		if (p->typ == Node::nt) {
			if (p->sym->firstReady) {
				fs->Or(p->sym->first);
			} else {
				BitArray *fs0 = First0(p->sym->graph, mark);
				fs->Or(fs0);
				delete fs0;
			}
		}
		else if (p->typ == Node::t || p->typ == Node::wt) {
			fs->Set(p->sym->n, true);
		}
		else if (p->typ == Node::any) {
			fs->Or(p->set);
		}
		else if (p->typ == Node::alt) {
			BitArray *fs0 = First0(p->sub, mark);
			fs->Or(fs0);
			delete fs0;
			fs0 = First0(p->down, mark);
			fs->Or(fs0);
			delete fs0;
		}
		else if (p->typ == Node::iter || p->typ == Node::opt) {
			BitArray *fs0 = First0(p->sub, mark);
			fs->Or(fs0);
			delete fs0;
		}

		if (!DelNode(p)) break;
		p = p->next;
	}
	return fs;
}

BitArray* Tab::First(const Node *p) {
	BitArray mark(nodes.Count);
	BitArray *fs = First0(p, &mark);
	if (ddt[3]) {
		fputws(_SC("\n"), trace);
		if (p != NULL) fwprintf(trace, _SC("First: node = %d\n"), p->n );
		else fputws(_SC("First: node = null\n"), trace);
		PrintSet(fs, 0);
	}
	return fs;
}


void Tab::CompFirstSets() {
	Symbol *sym;
	int i;
	for (i=0; i<nonterminals.Count; i++) {
		sym = nonterminals[i];
		delete sym->first;
		sym->first = new BitArray(terminals.Count);
		sym->firstReady = false;
	}
	for (i=0; i<nonterminals.Count; i++) {
		sym = nonterminals[i];
		BitArray *saved = sym->first;
		sym->first = First(sym->graph);
		delete saved;
		sym->firstReady = true;
	}
}

void Tab::CompFollow(Node *p) {
	while (p != NULL && !((*visited)[p->n])) {
		visited->Set(p->n, true);
		if (p->typ == Node::nt) {
			BitArray *s = First(p->next);
			p->sym->follow->Or(s);
			delete s;
			if (DelGraph(p->next))
				p->sym->nts->Set(curSy->n, true);
		} else if (p->typ == Node::opt || p->typ == Node::iter) {
			CompFollow(p->sub);
		} else if (p->typ == Node::alt) {
			CompFollow(p->sub); CompFollow(p->down);
		}
		p = p->next;
	}
}

void Tab::Complete(Symbol *sym) {
	if (!((*visited)[sym->n])) {
		visited->Set(sym->n, true);
		Symbol *s;
		for (int i=0; i<nonterminals.Count; i++) {
			s = nonterminals[i];
			if ((*(sym->nts))[s->n]) {
				Complete(s);
				sym->follow->Or(s->follow);
				if (sym == curSy) sym->nts->Set(s->n, false);
			}
		}
	}
}

void Tab::CompFollowSets() {
	Symbol *sym;
	int i;
	for (i=0; i<nonterminals.Count; i++) {
		sym = nonterminals[i];
		sym->follow = new BitArray(terminals.Count);
		sym->nts = new BitArray(nonterminals.Count);
	}
	gramSy->follow->Set(eofSy->n, true);
        delete visited;
	visited = new BitArray(nodes.Count);
	for (i=0; i<nonterminals.Count; i++) {  // get direct successors of nonterminals
		sym = nonterminals[i];
		curSy = sym;
		CompFollow(sym->graph);
	}

	for (i=0; i<nonterminals.Count; i++) {  // add indirect successors to followers
		sym = nonterminals[i];
		delete visited;
		visited = new BitArray(nonterminals.Count);
		curSy = sym;
		Complete(sym);
	}
}

const Node* Tab::LeadingAny(const Node *p) {
	if (p == NULL) return NULL;
	const Node *a = NULL;
	if (p->typ == Node::any) a = p;
	else if (p->typ == Node::alt) {
		a = LeadingAny(p->sub);
		if (a == NULL) a = LeadingAny(p->down);
	}
	else if (p->typ == Node::opt || p->typ == Node::iter) a = LeadingAny(p->sub);
	if (a == NULL && DelNode(p) && !p->up) a = LeadingAny(p->next);
	return a;
}

void Tab::FindAS(const Node *p) { // find ANY sets
	const Node *a;
	while (p != NULL) {
		if (p->typ == Node::opt || p->typ == Node::iter) {
			FindAS(p->sub);
			a = LeadingAny(p->sub);
			BitArray *ba = First(p->next);
			if (a != NULL) Sets::Subtract(a->set, ba);
			delete ba;
		} else if (p->typ == Node::alt) {
			BitArray s1(terminals.Count);
			const Node *q = p;
			while (q != NULL) {
				FindAS(q->sub);
				a = LeadingAny(q->sub);
				if (a != NULL) {
					BitArray *tmp = First(q->down);
					tmp->Or(&s1);
					Sets::Subtract(a->set, tmp);
					delete tmp;
				} else {
					BitArray *f = First(q->sub);
					s1.Or(f);
					delete f;
				}
				q = q->down;
			}
		}

		// Remove alternative terminals before ANY, in the following
		// examples a and b must be removed from the ANY set:
		// [a] ANY, or {a|b} ANY, or [a][b] ANY, or (a|) ANY, or
		// A = [a]. A ANY
		if (DelNode(p)) {
			a = LeadingAny(p->next);
			if (a != NULL) {
				Node *q = (p->typ == Node::nt) ? p->sym->graph : p->sub;
                                BitArray *ba = First(q);
				Sets::Subtract(a->set, ba);
                                delete ba;
			}
		}

		if (p->up) break;
		p = p->next;
	}
}

void Tab::CompAnySets() {
	Symbol *sym;
	for (int i=0; i<nonterminals.Count; i++) {
		sym = nonterminals[i];
		FindAS(sym->graph);
	}
}

BitArray* Tab::Expected(const Node *p, const Symbol *curSy) {
	BitArray *s = First(p);
	if (DelGraph(p))
		s->Or(curSy->follow);
	return s;
}

// does not look behind resolvers; only called during LL(1) test and in CheckRes
BitArray* Tab::Expected0(const Node *p, const Symbol *curSy) {
	if (p->typ == Node::rslv) return new BitArray(terminals.Count);
	else return Expected(p, curSy);
}

void Tab::CompSync(Node *p) {
	while (p != NULL && !(visited->Get(p->n))) {
		visited->Set(p->n, true);
		if (p->typ == Node::sync) {
			BitArray *s = Expected(p->next, curSy);
			s->Set(eofSy->n, true);
			allSyncSets->Or(s);
			p->set = s;
		} else if (p->typ == Node::alt) {
			CompSync(p->sub); CompSync(p->down);
		} else if (p->typ == Node::opt || p->typ == Node::iter)
			CompSync(p->sub);
		p = p->next;
	}
}

void Tab::CompSyncSets() {
	allSyncSets = new BitArray(terminals.Count);
	allSyncSets->Set(eofSy->n, true);
        delete visited;
	visited = new BitArray(nodes.Count);

	Symbol *sym;
	for (int i=0; i<nonterminals.Count; i++) {
		sym = nonterminals[i];
		curSy = sym;
		CompSync(curSy->graph);
	}
}

void Tab::SetupAnys() {
	Node *p;
	for (int i=0; i<nodes.Count; i++) {
		p = nodes[i];
		if (p->typ == Node::any) {
			p->set = new BitArray(terminals.Count, true);
			p->set->Set(eofSy->n, false);
		}
	}
}

void Tab::CompDeletableSymbols() {
	bool changed;
	Symbol *sym;
	int i;
	do {
		changed = false;
		for (i=0; i<nonterminals.Count; i++) {
			sym = nonterminals[i];
			if (!sym->deletable && sym->graph != NULL && DelGraph(sym->graph)) {
				sym->deletable = true; changed = true;
			}
		}
	} while (changed);

	for (i=0; i<nonterminals.Count; i++) {
		sym = nonterminals[i];
		if (sym->deletable)
			wprintf(_SC("  %") _SFMT _SC(" deletable\n"),  sym->name);
	}
}

void Tab::RenumberPragmas() {
	int n = terminals.Count;
	Symbol *sym;
	for (int i=0; i<pragmas.Count; i++) {
		sym = pragmas[i];
		sym->n = n++;
	}
}

void Tab::CompSymbolSets() {
	CompDeletableSymbols();
	CompFirstSets();
	CompAnySets();
	CompFollowSets();
	CompSyncSets();
	if (ddt[1]) {
		fwprintf(trace, _SC("%s"),
                        "\n"
                        "First & follow symbols:\n"
                        "----------------------\n\n");

		Symbol *sym;
		for (int i=0; i<nonterminals.Count; i++) {
			sym = nonterminals[i];
			fwprintf(trace, _SC("%") _SFMT _SC("\n"), sym->name);
			fputws(_SC("first:   "), trace); PrintSet(sym->first, 10);
			fputws(_SC("follow:  "), trace); PrintSet(sym->follow, 10);
			fputws(_SC("\n"), trace);
		}
	}
	if (ddt[4]) {
		fwprintf(trace, _SC("%s"),
                        "\n"
                        "ANY and SYNC sets:\n"
                        "-----------------\n");

		Node *p;
		for (int i=0; i<nodes.Count; i++) {
			p = nodes[i];
			if (p->typ == Node::any || p->typ == Node::sync) {
				fwprintf(trace, _SC("%4d %4s "), p->n, nTyp[p->typ]);
				PrintSet(p->set, 11);
			}
		}
	}
}

//---------------------------------------------------------------------
//  String handling
//---------------------------------------------------------------------

int Tab::Hex2Char(const wchar_t* s, int len) {
	int val = 0;
	for (int i = 0; i < len; i++) {
		wchar_t ch = s[i];
		if ('0' <= ch && ch <= '9') val = 16 * val + (ch - '0');
		else if ('a' <= ch && ch <= 'f') val = 16 * val + (10 + ch - 'a');
		else if ('A' <= ch && ch <= 'F') val = 16 * val + (10 + ch - 'A');
		else parser->SemErr(_SC("bad escape sequence in string or character"));
	}
	if (val > COCO_WCHAR_MAX) {/* pdt */
		parser->SemErr(_SC("bad escape sequence in string or character"));
	}
	return val;
}

static wchar_t* TabChar2Hex(const wchar_t ch, wchar_t_10 &format) {
	coco_swprintf(format, wchar_t_10_sz, _SC("\\0x%04x"), ch);
	return format;
}

wchar_t* Tab::Unescape (const wchar_t* s) {
	/* replaces escape sequences in s by their Unicode values. */
	StringBuilder buf;
	int i = 0;
	int len = coco_string_length(s);
	while (i < len) {
		if (s[i] == _SC('\\')) {
			switch (s[i+1]) {
				case _SC('\\'): buf.Append(_SC('\\')); i += 2; break;
				case _SC('\''): buf.Append(_SC('\'')); i += 2; break;
				case _SC('\"'): buf.Append(_SC('\"')); i += 2; break;
				case _SC('r'): buf.Append(_SC('\r')); i += 2; break;
				case _SC('n'): buf.Append(_SC('\n')); i += 2; break;
				case _SC('t'): buf.Append(_SC('\t')); i += 2; break;
				case _SC('0'): buf.Append(_SC('\0')); i += 2; break;
				case _SC('a'): buf.Append(_SC('\a')); i += 2; break;
				case _SC('b'): buf.Append(_SC('\b')); i += 2; break;
				case _SC('f'): buf.Append(_SC('\f')); i += 2; break;
				case _SC('v'): buf.Append(_SC('\v')); i += 2; break;
				case _SC('u'): case _SC('x'):
					if (i + 6 <= coco_string_length(s)) {
						buf.Append(Hex2Char(s +i+2, 4)); i += 6; break;
					} else {
						parser->SemErr(_SC("bad escape sequence in string or character"));
						i = coco_string_length(s); break;
					}
				default:
						parser->SemErr(_SC("bad escape sequence in string or character"));
					i += 2; break;
			}
		} else {
			buf.Append(s[i]);
			i++;
		}
	}

	return buf.ToString();
}


wchar_t* Tab::Escape (const wchar_t* s) {
	StringBuilder buf;
	wchar_t ch;
	int len = coco_string_length(s);
        wchar_t_10 fmt;
	for (int i=0; i < len; i++) {
		ch = s[i];
		switch(ch) {
			case _SC('\\'): buf.Append(_SC("\\\\")); break;
			case _SC('\''): buf.Append(_SC("\\'")); break;
			case _SC('\"'): buf.Append(_SC("\\\"")); break;
			case _SC('\t'): buf.Append(_SC("\\t")); break;
			case _SC('\r'): buf.Append(_SC("\\r")); break;
			case _SC('\n'): buf.Append(_SC("\\n")); break;
			default:
				if ((ch < _SC(' ')) || (ch > 0x7f)) {
					wchar_t* res = TabChar2Hex(ch, fmt);
					buf.Append(res);
				} else
					buf.Append(ch);
				break;
		}
	}
	return buf.ToString();
}


//---------------------------------------------------------------------
//  Grammar checks
//---------------------------------------------------------------------

bool Tab::GrammarOk() {
	bool ok = NtsComplete()
		&& AllNtReached()
		&& NoCircularProductions()
		&& AllNtToTerm();
    if (ok) { CheckResolvers(); CheckLL1(); }
    return ok;
}

bool Tab::GrammarCheckAll() {
        int errors = 0;
        if(!NtsComplete()) ++errors;
        if(!AllNtReached()) ++errors;
        if(!NoCircularProductions()) exit(1);
        if(!AllNtToTerm()) ++errors;
        CheckResolvers(); CheckLL1();
        return errors == 0;
}

//--------------- check for circular productions ----------------------

void Tab::GetSingles(const Node *p, TArrayList<Symbol*> &singles) {
	if (p == NULL) return;  // end of graph
	if (p->typ == Node::nt) {
                singles.Add(p->sym);
	} else if (p->typ == Node::alt || p->typ == Node::iter || p->typ == Node::opt) {
		if (p->up || DelGraph(p->next)) {
			GetSingles(p->sub, singles);
			if (p->typ == Node::alt) GetSingles(p->down, singles);
		}
	}
	if (!p->up && DelNode(p)) GetSingles(p->next, singles);
}

bool Tab::NoCircularProductions() {
	bool ok, changed, onLeftSide, onRightSide;
	TArrayList<CNode*> list;


	Symbol *sym;
	int i;
	for (i=0; i<nonterminals.Count; i++) {
		sym = nonterminals[i];
		TArrayList<Symbol*> singles;
		GetSingles(sym->graph, singles); // get nonterminals s such that sym-->s
		Symbol *s;
		for (int j=0; j<singles.Count; j++) {
			s = singles[j];
			list.Add(new CNode(sym, s));
		}
	}

	CNode *n;
	do {
		changed = false;
		for (i = 0; i < list.Count; i++) {
			n = list[i];
			onLeftSide = false; onRightSide = false;
			CNode *m;
			for (int j=0; j<list.Count; j++) {
				m = list[j];
				if (n->left == m->right) onRightSide = true;
				if (n->right == m->left) onLeftSide = true;
			}
			if (!onLeftSide || !onRightSide) {
                                delete n;
				list.Remove(n); i--; changed = true;
			}
		}
	} while(changed);
	ok = true;

	for (i=0; i<list.Count; i++) {
		n = list[i];
			ok = false; errors->count++;
		wprintf(_SC("  %") _SFMT _SC(":%d --> %") _SFMT _SC(":%d\n"), n->left->name, n->left->line, n->right->name, n->right->line);
	}
        for(int i=0; i<list.Count; ++i) delete list[i];
	return ok;
}


//--------------- check for LL(1) errors ----------------------

void Tab::LL1Error(int cond, const Symbol *sym) {
	wprintf(_SC("  LL1 warning in %") _SFMT _SC(":%d:%d: "), curSy->name, curSy->line, curSy->col);
	if (sym != NULL) wprintf(_SC("%") _SFMT _SC(" is "), sym->name);
	switch (cond) {
		case 1: wprintf(_SC("%s"), "start of several alternatives\n"); break;
		case 2: wprintf(_SC("%s"), "start & successor of deletable structure\n"); break;
		case 3: wprintf(_SC("%s"), "an ANY node that matches no symbol\n"); break;
		case 4: wprintf(_SC("%s"), "contents of [...] or {...} must not be deletable\n"); break;
	}
}


int Tab::CheckOverlap(const BitArray *s1, const BitArray *s2, int cond) {
        int overlaped = 0;
	Symbol *sym;
	for (int i=0; i<terminals.Count; i++) {
		sym = terminals[i];
		if ((*s1)[sym->n] && (*s2)[sym->n]) {
			LL1Error(cond, sym);
                        ++overlaped;
		}
	}
        return overlaped;
}

/* print the path for first set that contains token tok for the graph rooted at p */
void Tab::PrintFirstPath(const Node *p, int tok, const wchar_t *indent) {
        while (p != NULL) {
        //if(p->sym) wprintf(_SC("%") _SFMT _SC("-> %") _SFMT _SC(":%d:\n", indent, p->sym->name, p->sym->line));
                switch (p->typ) {
                        case Node::nt: {
                                if (p->sym->firstReady) {
                                        if(p->sym->first->Get(tok)) {
                                                if(coco_string_length(indent) == 1)
                                                    wprintf(_SC("%") _SFMT _SC("=> %") _SFMT _SC(":%d:%d:\n"), indent, p->sym->name, p->line, p->col);
                                                wprintf(_SC("%") _SFMT _SC("-> %") _SFMT _SC(":%d:%d:\n"), indent, p->sym->name, p->sym->line, p->sym->col);
                                                if(p->sym->graph) {
                                                    wchar_t *new_indent = coco_string_create_append(indent, _SC("  "));
                                                    PrintFirstPath(p->sym->graph, tok, new_indent);
                                                    coco_string_delete(new_indent);
                                                }
                                                return;
                                        }
                                }
                                break;
                        }
                        case Node::t: case Node::wt: {
                                if(p->sym->n == tok)
                                    wprintf(_SC("%") _SFMT _SC("= %") _SFMT _SC(":%d:%d:\n"), indent, p->sym->name, p->line, p->col);
                                break;
                        }
                        case Node::any: {
                                break;
                        }
                        case Node::alt: {
                                PrintFirstPath(p->sub, tok, indent);
                                PrintFirstPath(p->down, tok, indent);
                                break;
                        }
                        case Node::iter: case Node::opt: {
				if (!DelNode(p->sub)) //prevent endless loop with some ill grammars
                                    PrintFirstPath(p->sub, tok, indent);
                                break;
                        }
                }
                if (!DelNode(p)) break;
                p = p->next;
        }
}

int Tab::CheckAlts(Node *p) {
        int rc = 0;
	BitArray s0(terminals.Count), *s1, *s2;
	while (p != NULL) {
		if (p->typ == Node::alt) {
			Node *q = p;
			s0.SetAll(false);
			while (q != NULL) { // for all alternatives
				s2 = Expected0(q->sub, curSy);
				int overlaped = CheckOverlap(&s0, s2, 1);
                                if(overlaped > 0) {
                                        int overlapToken = 0;
                                        /* Find the first overlap token */
                                        for (int i=0; i<terminals.Count; i++) {
                                                Symbol *sym = terminals[i];
                                                if (s0.Get(sym->n) && s2->Get(sym->n)) {overlapToken = sym->n; break;}
                                        }
                                        //print(format("\t-> %s:%d: %d", first_overlap.sub.sym.name, first_overlap.sub.sym.line, overlaped));
                                        PrintFirstPath( p, overlapToken);
                                        rc += overlaped;
                                }
				s0.Or(s2);
                                delete s2;
				CheckAlts(q->sub);
				q = q->down;
			}
		} else if (p->typ == Node::opt || p->typ == Node::iter) {
			if (DelSubGraph(p->sub)) LL1Error(4, NULL); // e.g. [[...]]
			else {
				s1 = Expected0(p->sub, curSy);
				s2 = Expected(p->next, curSy);
				int overlaped = CheckOverlap(s1, s2, 2);
                                if(overlaped > 0) {
                                        int overlapToken = 0;
                                        /* Find the first overlap token */
                                        for (int i=0; i<terminals.Count; i++) {
                                                Symbol *sym = terminals[i];
                                                if (s1->Get(sym->n) && s2->Get(sym->n)) {overlapToken = sym->n; break;}
                                        }
                                        //print(format("\t=>:%d: %d", p.line, overlaped));
                                        PrintFirstPath(p, overlapToken);
                                        rc += overlaped;
                                }
                                delete s1; delete s2;
			}
			CheckAlts(p->sub);
		} else if (p->typ == Node::any) {
			if (Sets::Elements(p->set) == 0) LL1Error(3, NULL);
			// e.g. {ANY} ANY or [ANY] ANY or ( ANY | ANY )
		}
		if (p->up) break;
		p = p->next;
	}
        return rc;
}

void Tab::CheckLL1() {
	Symbol *sym;
	for (int i=0; i<nonterminals.Count; i++) {
		sym = nonterminals[i];
		curSy = sym;
		CheckAlts(curSy->graph);
	}
}

//------------- check if resolvers are legal  --------------------

void Tab::ResErr(const Node *p, const wchar_t* msg) {
	errors->Warning(p->line, p->pos->col, msg);
}

void Tab::CheckRes(const Node *p, bool rslvAllowed) {
	BitArray expected(terminals.Count), soFar(terminals.Count);
	while (p != NULL) {

		const Node *q;
		if (p->typ == Node::alt) {
			expected.SetAll(false);
			for (q = p; q != NULL; q = q->down) {
				BitArray *ba = Expected0(q->sub, curSy);
				expected.Or(ba);
				delete ba;
                        }
			soFar.SetAll(false);
			for (q = p; q != NULL; q = q->down) {
				if (q->sub->typ == Node::rslv) {
					BitArray *fs = Expected(q->sub->next, curSy);
					if (Sets::Intersect(fs, &soFar))
						ResErr(q->sub, _SC("Warning: Resolver will never be evaluated. Place it at previous conflicting alternative."));
					if (!Sets::Intersect(fs, &expected))
						ResErr(q->sub, _SC("Warning: Misplaced resolver: no LL(1) conflict."));
                                        delete fs;
				} else {
                                    BitArray *ba = Expected(q->sub, curSy);
                                    soFar.Or(ba);
                                    delete ba;
                                }
				CheckRes(q->sub, true);
			}
		} else if (p->typ == Node::iter || p->typ == Node::opt) {
			if (p->sub->typ == Node::rslv) {
				BitArray *fs = First(p->sub->next);
				BitArray *fsNext = Expected(p->next, curSy);
                                bool bsi = Sets::Intersect(fs, fsNext);
                                delete fs; delete fsNext;
				if (!bsi)
					ResErr(p->sub, _SC("Warning: Misplaced resolver: no LL(1) conflict."));
			}
			CheckRes(p->sub, true);
		} else if (p->typ == Node::rslv) {
			if (!rslvAllowed)
				ResErr(p, _SC("Warning: Misplaced resolver: no alternative."));
		}

		if (p->up) break;
		p = p->next;
		rslvAllowed = false;
	}
}

void Tab::CheckResolvers() {
	for (int i=0; i<nonterminals.Count; i++) {
		curSy = nonterminals[i];
		CheckRes(curSy->graph, false);
	}
}


//------------- check if every nts has a production --------------------

bool Tab::NtsComplete() {
	bool complete = true;
	Symbol *sym;
	for (int i=0; i<nonterminals.Count; i++) {
		sym = nonterminals[i];
		if (sym->graph == NULL) {
				complete = false; errors->count++;
			wprintf(_SC("  No production for %") _SFMT _SC("\n"), sym->name);
		}
	}
	return complete;
}

//-------------- check if every nts can be reached  -----------------

void Tab::MarkReachedNts(const Node *p) {
	while (p != NULL) {
		if (p->typ == Node::nt && !((*visited)[p->sym->n])) { // new nt reached
			visited->Set(p->sym->n, true);
			MarkReachedNts(p->sym->graph);
		} else if (p->typ == Node::alt || p->typ == Node::iter || p->typ == Node::opt) {
			MarkReachedNts(p->sub);
			if (p->typ == Node::alt) MarkReachedNts(p->down);
		}
		if (p->up) break;
		p = p->next;
	}
}

bool Tab::AllNtReached() {
	bool ok = true;
        delete visited;
	visited = new BitArray(nonterminals.Count);
	visited->Set(gramSy->n, true);
	MarkReachedNts(gramSy->graph);
	Symbol *sym;
	for (int i=0; i<nonterminals.Count; i++) {
		sym = nonterminals[i];
		if (!((*visited)[sym->n])) {
				ok = false; errors->count++;
			wprintf(_SC("  %") _SFMT _SC(" cannot be reached\n"), sym->name);
		}
	}
	return ok;
}

//--------- check if every nts can be derived to terminals  ------------

bool Tab::IsTerm(const Node *p, const BitArray *mark) { // true if graph can be derived to terminals
	while (p != NULL) {
		if (p->typ == Node::nt && !((*mark)[p->sym->n])) return false;
		if (p->typ == Node::alt && !IsTerm(p->sub, mark)
		&& (p->down == NULL || !IsTerm(p->down, mark))) return false;
		if (p->up) break;
		p = p->next;
	}
	return true;
}


bool Tab::AllNtToTerm() {
	bool changed, ok = true;
	BitArray mark(nonterminals.Count);
	// a nonterminal is marked if it can be derived to terminal symbols
	Symbol *sym;
	int i;
	do {
		changed = false;

		for (i=0; i<nonterminals.Count; i++) {
			sym = nonterminals[i];
			if (!mark[sym->n] && IsTerm(sym->graph, &mark)) {
				mark.Set(sym->n, true); changed = true;
			}
		}
	} while (changed);
	for (i=0; i<nonterminals.Count; i++) {
		sym = nonterminals[i];
		if (!mark[sym->n]) {
				ok = false; errors->count++;
			wprintf(_SC("  %") _SFMT _SC(" cannot be derived to terminals\n"), sym->name);
		}
	}
	return ok;
}

//---------------------------------------------------------------------
//  Cross reference list
//---------------------------------------------------------------------

void Tab::XRef() {
	SortedList xref;
	// collect lines where symbols have been defined
	Symbol *sym;
	int i, j;
	for (i=0; i<nonterminals.Count; i++) {
		sym = nonterminals[i];
		TArrayList<int> *list = (TArrayList<int>*)(xref.Get(sym));
		if (list == NULL) {list = new TArrayList<int>(); xref.Set(sym, list);}
		list->Add(-sym->line);
	}
	// collect lines where symbols have been referenced
	Node *n;
	for (i=0; i<nodes.Count; i++) {
		n = nodes[i];
		if (n->typ == Node::t || n->typ == Node::wt || n->typ == Node::nt) {
			TArrayList<int> *list = (TArrayList<int>*)(xref.Get(n->sym));
			if (list == NULL) {list = new TArrayList<int>(); xref.Set(n->sym, list);}
			list->Add(n->line);
		}
	}
	// print cross reference list
	fwprintf(trace, _SC("%s"),
                "\n"
                "Cross reference list:\n"
                "--------------------\n\n");

	for (i=0; i<xref.Count; i++) {
		sym = (Symbol*)(xref.GetKey(i));
		fwprintf(trace, _SC("  %-12.12") _SFMT, sym->name);
		TArrayList<int> *list = (TArrayList<int>*)(xref.Get(sym));
		int col = 14;
		int line;
		for (j=0; j<list->Count; j++) {
			line = (*list)[j];
			if (col + 5 > 80) {
				fputws(_SC("\n"), trace);
				for (col = 1; col <= 14; col++) fputws(_SC(" "), trace);
			}
			fwprintf(trace, _SC("%5d"), line); col += 5;
		}
		fputws(_SC("\n"), trace);
	}
	fputws(_SC("\n\n"), trace);
        for(int i=0; i < xref.Count; ++i) {
            SortedEntry *se = xref[i];
            /*
            while(se->next) {
                SortedEntry *tmp = se->next;
                delete (ArrayList*)tmp->Value;
                se->next = tmp;
            }
            */
            delete (TArrayList<int>*)se->Value;
        }
}

void Tab::SetDDT(const wchar_t* s) {
	wchar_t* st = coco_string_create_upper(s);
	wchar_t ch;
	int len = coco_string_length(st);
	for (int i = 0; i < len; i++) {
		ch = st[i];
		if (_SC('0') <= ch && ch <= _SC('9')) ddt[ch - _SC('0')] = true;
		else switch (ch) {
			case _SC('A') : ddt[0] = true; break; // trace automaton
			case _SC('F') : ddt[1] = true; break; // list first/follow sets
			case _SC('G') : ddt[2] = true; break; // print syntax graph
			case _SC('I') : ddt[3] = true; break; // trace computation of first sets
			case _SC('J') : ddt[4] = true; break; // print ANY and SYNC sets
			case _SC('P') : ddt[8] = true; break; // print statistics
			case _SC('S') : ddt[6] = true; break; // list symbol table
			case _SC('X') : ddt[7] = true; break; // list cross reference table
			default : break;
		}
	}
	coco_string_delete(st);
}


void Tab::SetOption(const wchar_t* s) {
	// example: $namespace=xxx
	//   index of '=' is 10 => nameLenght = 10
	//   start index of xxx = 11

	int nameLenght = coco_string_indexof(s, '=');
	int valueIndex = nameLenght + 1;

	if (coco_string_equal_n(_SC("$namespace"), s, nameLenght)) {
		if (nsName == NULL) nsName = coco_string_create(s + valueIndex);
	} else if (coco_string_equal_n(_SC("$checkEOF"), s, nameLenght)) {
		checkEOF = coco_string_equal(_SC("true"), s + valueIndex);
	}
}


}; // namespace
