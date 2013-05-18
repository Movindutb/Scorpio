#include "scorpio.h"

/*
MAKE MOVE
*/
void SEARCHER::do_move(const MOVE& move) {
	int from = m_from(move),to = m_to(move),sq,pic,pic1;

	/*save state*/
	PHIST_STACK phstack = hstack + hply;
	phstack->move = move;
	phstack->castle = castle;
	phstack->epsquare = epsquare;
	phstack->fifty = fifty;
	phstack->checks = checks(move,phstack->rev_check);
	phstack->pcsq_score[white] = pcsq_score[white];
	phstack->pcsq_score[black] = pcsq_score[black];
	phstack->all_bb = all_bb;
	phstack->hash_key = hash_key;
    phstack->pawn_hash_key = pawn_hash_key;
	phstack->pieces_bb[white] = pieces_bb[white];
	phstack->pieces_bb[black] = pieces_bb[black];
	phstack->pawns_bb[white] = pawns_bb[white];
	phstack->pawns_bb[black] = pawns_bb[black];

	/*remove captured piece*/
	if((pic = m_capture(move)) != 0) {
		if(is_ep(move)) {
			sq = to - pawn_dir[player];
			all_bb ^= BB(sq);
		}
		else sq = to;
		pcRemove(pic,sq,phstack->pCapt);
		board[sq] = blank;

		hash_key ^= PC_HKEY(pic,sq);
		if(PIECE(pic) == pawn) {
			pawn_hash_key ^= PC_HKEY(pic,sq);
			pawns_bb[opponent] ^= BB(sq);
			pawn_c[opponent]--;
		} else {
            pieces_bb[opponent] ^= BB(sq);
            piece_c[opponent] -= piece_cv[pic];
		}
		pcsq_score[opponent].sub(pcsq[pic][sq],pcsq[pic][sq + 8]);
		man_c[pic]--;
		all_man_c--;
	}

	/*move piece*/
	all_bb ^= BB(from);
	all_bb |= BB(to);
	if((pic = m_promote(move)) != 0) {
		pic1 = COMBINE(player,pawn);
		board[to] = pic;
		board[from] = blank;
		pcAdd(pic,to);
		pcRemove(pic1,from,phstack->pProm);

		pawns_bb[player] ^= BB(from);
		pieces_bb[player] ^= BB(to);
		hash_key      ^= PC_HKEY(pic1,from);
		pawn_hash_key ^= PC_HKEY(pic1,from);
	    hash_key      ^= PC_HKEY(pic,to);

		pawn_c[player]--;
		piece_c[player] += piece_cv[pic];
        pcsq_score[player].add(pcsq[pic][to] - pcsq[pic1][from],
			                   pcsq[pic][to + 8] - pcsq[pic1][from + 8]);
		man_c[pic1]--;
		man_c[pic]++;
	} else {
		pic = m_piece(move);
		board[to] = board[from];
		board[from] = blank;
		pcSwap(from,to);

		hash_key ^= PC_HKEY(pic,to);
		hash_key ^= PC_HKEY(pic,from);
		if(PIECE(pic) == pawn) {
			pawns_bb[player] ^= (BB(from) | BB(to));
			pawn_hash_key ^= PC_HKEY(pic,to);
			pawn_hash_key ^= PC_HKEY(pic,from);
		} else {
            pieces_bb[player] ^= (BB(from) | BB(to));
		}
		pcsq_score[player].add(pcsq[pic][to] - pcsq[pic][from],
			                   pcsq[pic][to + 8] - pcsq[pic][from + 8]);
	}

	/*move castle*/
	if(is_castle(move)) {
        int fromc,toc;
		if(to > from) {
           fromc = to + RR;
		   toc = to + LL;
		} else {
           fromc = to + LLL;
		   toc = to + RR;
		}
		
		board[toc] = board[fromc];
		board[fromc] = blank;
		pcSwap(fromc,toc);
		pic = COMBINE(player,rook);

		pieces_bb[player] ^= (BB(fromc) | BB(toc));
		all_bb ^= (BB(fromc) | BB(toc));
		hash_key ^= PC_HKEY(pic,toc);
		hash_key ^= PC_HKEY(pic,fromc);

		pcsq_score[player].add(pcsq[pic][toc] - pcsq[pic][fromc],
			                   pcsq[pic][toc + 8] - pcsq[pic][fromc + 8]);
	} 

	/*enpassant*/
	if(epsquare) hash_key ^= EP_HKEY(epsquare);
	epsquare = 0;

	/*fifty moves*/
	fifty++;
	if(PIECE(m_piece(move)) == pawn) {
		fifty = 0;
	    if(to - from == (2 * pawn_dir[player])) {
            epsquare = ((to + from) >> 1);
			hash_key ^= EP_HKEY(epsquare);
		}
	} else if(m_capture(move)) {
		fifty = 0;
	}

    /*castling*/
	if(castle) {
		int p_castle = castle;
		if(from == E1 || to == A1 || from == A1) castle &= ~WLC_FLAG;
		if(from == E1 || to == H1 || from == H1) castle &= ~WSC_FLAG;
		if(from == E8 || to == A8 || from == A8) castle &= ~BLC_FLAG;
		if(from == E8 || to == H8 || from == H8) castle &= ~BSC_FLAG;
		if(p_castle != castle) {
			hash_key ^= CAST_HKEY(p_castle);
			hash_key ^= CAST_HKEY(castle);
		}
	}

	player = invert(player);
	opponent = invert(opponent);

    hply++;
}
/*
UNDO MOVE
*/
void SEARCHER::undo_move() {
	MOVE move;
	int to,from,sq,pic,pic1;
	PLIST pTemp;

	hply--;

	player = invert(player);
	opponent = invert(opponent);

	/*retrieve state*/
	PHIST_STACK phstack = &hstack[hply];
	move = phstack->move;
	castle = phstack->castle;
	epsquare = phstack->epsquare;
	fifty = phstack->fifty;
	pcsq_score[white] = phstack->pcsq_score[white];
	pcsq_score[black] = phstack->pcsq_score[black];
	all_bb = phstack->all_bb;
	hash_key = phstack->hash_key;
    pawn_hash_key = phstack->pawn_hash_key;
	pieces_bb[white] = phstack->pieces_bb[white];
	pieces_bb[black] = phstack->pieces_bb[black];
	pawns_bb[white] = phstack->pawns_bb[white];
	pawns_bb[black] = phstack->pawns_bb[black];
	
	to = m_to(move);
	from = m_from(move);

	/*unmove castle*/
	if(is_castle(move)) {
        int fromc,toc;
		if(to > from) {
           fromc = to + LL;
		   toc = to + RR;
		} else {
           fromc = to + RR;
		   toc = to + LLL;
		}
		board[toc] = board[fromc];
		board[fromc] = blank;
		pcSwap(fromc,toc);
		pic = COMBINE(player,rook);
	} 

	/*unmove piece*/
	if((pic = m_promote(move)) != 0) {
		pic1 = COMBINE(player,pawn);
		board[from] = pic1;
		board[to] = blank;
		pcAdd(pic1,from,phstack->pProm);
		pcRemove(pic,to,pTemp);

        pawn_c[player]++;
		piece_c[player] -= piece_cv[pic];
		man_c[pic1]++;
		man_c[pic]--;
	} else {
		board[from] = board[to];
		board[to] = blank;
		pcSwap(to,from);
		pic = m_piece(move);
	}

	/*insert captured piece*/
	if((pic = m_capture(move)) != 0) {
		if(is_ep(move)) sq = to - pawn_dir[player];
		else sq = to;
		board[sq] = pic;
		pcAdd(pic,sq,phstack->pCapt);

		if(PIECE(pic) == pawn) pawn_c[opponent]++;
		else piece_c[opponent] += piece_cv[pic];
		man_c[pic]++;
		all_man_c++;
	}
}
/*
MAKE NULL MOVE
*/
void SEARCHER::do_null() {
	PHIST_STACK phstack = hstack + hply;
	phstack->move = 0;
	phstack->epsquare = epsquare;
	phstack->fifty = fifty;
	phstack->checks = 0;
	phstack->hash_key = hash_key;

	if(epsquare)
	    hash_key ^= EP_HKEY(epsquare);
	epsquare = 0;
	fifty++;

	player = invert(player);
	opponent = invert(opponent);

    hply++;
}
/*
UNMAKE NULL MOVE
*/
void SEARCHER::undo_null() {
	hply--;

	player = invert(player);
	opponent = invert(opponent);

	PHIST_STACK phstack = &hstack[hply];
	epsquare = phstack->epsquare;
	fifty = phstack->fifty;
	hash_key = phstack->hash_key;
}
/*
Generate captures
*/
#define CAPS() {                                                        \
		while(bb) {														\
            to = first_one(bb);                                         \
			*pmove++ = tmove | (to<<8) | (board[to]<<20);               \
			bb &= NBB(to);                                              \
		}                                                               \
}

void SEARCHER::gen_caps(bool recap) {
	register BITBOARD bb;
	BITBOARD occupancyw = (pieces_bb[white] | pawns_bb[white]);
	BITBOARD occupancyb = (pieces_bb[black] | pawns_bb[black]);
	BITBOARD occupancy = (occupancyw | occupancyb);
	MOVE* pmove = &pstack->move_st[pstack->count],*spmove = pmove,tmove;
	int  from,to;
	PLIST current;
	
	if(player == white) {
		if(recap)
			occupancyb = BB(m_to(hstack[hply - 1].move));
		/*pawns*/
		bb = ((pawns_bb[white] & ~file_mask[FILEA]) << 7) & occupancyb;
		while(bb) {														
            to = first_one(bb);        
			from = to + RD;
			if(rank(to) == RANK8) {
				tmove = from | (to<<8) | (wpawn<<16) | (board[to]<<20);
				*pmove++ = tmove | (wqueen<<24);
				*pmove++ = tmove | (wknight<<24);
				*pmove++ = tmove | (wrook<<24);
				*pmove++ = tmove | (wbishop<<24);
			} else {
				*pmove++ = from | (to<<8) | (wpawn<<16) | (board[to]<<20);
			}
			bb &= NBB(to);                                              
		}
		bb = ((pawns_bb[white] & ~file_mask[FILEH]) << 9) & occupancyb;
		while(bb) {														
            to = first_one(bb);        
			from = to + LD;
			if(rank(to) == RANK8) {
				tmove = from | (to<<8) | (wpawn<<16) | (board[to]<<20);
				*pmove++ = tmove | (wqueen<<24);
				*pmove++ = tmove | (wknight<<24);
				*pmove++ = tmove | (wrook<<24);
				*pmove++ = tmove | (wbishop<<24);
			} else {
				*pmove++ = from | (to<<8) | (wpawn<<16) | (board[to]<<20);
			}
			bb &= NBB(to);                                              
		}
		bb = ((pawns_bb[white] & rank_mask[RANK7]) << 8) & ~occupancy;
		while(bb) {														
            to = first_one(bb);        
			from = to + DD;
			*pmove++ = from | (to<<8) | (wpawn<<16) | (wqueen<<24);
			bb &= NBB(to);                                              
		}
		if(epsquare) {
			from = epsquare + LD;
			if(board[from] == wpawn)
				*pmove++ = from | (epsquare<<8) | (wpawn<<16) | (bpawn<<20) | EP_FLAG;
			
			from = epsquare + RD;
			if(board[from] == wpawn)
				*pmove++ = from | (epsquare<<8) | (wpawn<<16) | (bpawn<<20) | EP_FLAG;
		}
		/*knight*/
		current = plist[wknight];
		while(current) {
			from = current->sq;
			tmove = from | (wknight<<16);
			bb = knight_attacks(SQ8864(from)) & occupancyb;
			CAPS();
			current = current->next;
		}
		/*bishop*/
		current = plist[wbishop];
		while(current) {
			from = current->sq;
			tmove = from | (wbishop<<16);
			bb =  bishop_attacks(SQ8864(from),occupancy) & occupancyb;
			CAPS();
			current = current->next;
		}
		/*rook*/
		current = plist[wrook];
		while(current) {
			from = current->sq;
			tmove = from | (wrook<<16);
			bb =  rook_attacks(SQ8864(from),occupancy) & occupancyb;
			CAPS();
			current = current->next;
		}
		/*queen*/
		current = plist[wqueen];
		while(current) {
			from = current->sq;
			tmove = from | (wqueen<<16);
			bb =  queen_attacks(SQ8864(from),occupancy) & occupancyb;
			CAPS();
			current = current->next;
		}
		/*king*/
		from = plist[wking]->sq;
		tmove = from | (wking<<16);
		bb = king_attacks(SQ8864(from)) & occupancyb;
		CAPS();
	} else {
		if(recap)
			occupancyw = BB(m_to(hstack[hply - 1].move));
		/*pawns*/
		bb = ((pawns_bb[black] & ~file_mask[FILEH]) >> 7) & occupancyw;
		while(bb) {														
            to = first_one(bb);        
			from = to + LU;
			if(rank(to) == RANK1) {
				tmove = from | (to<<8) | (bpawn<<16) | (board[to]<<20);
				*pmove++ = tmove | (bqueen<<24);
				*pmove++ = tmove | (bknight<<24);
				*pmove++ = tmove | (brook<<24);
				*pmove++ = tmove | (bbishop<<24);
			} else {
				*pmove++ = from | (to<<8) | (bpawn<<16) | (board[to]<<20);
			}
			bb &= NBB(to);                                              
		}
		bb = ((pawns_bb[black] & ~file_mask[FILEA]) >> 9) & occupancyw;
		while(bb) {														
            to = first_one(bb);        
			from = to + RU;
			if(rank(to) == RANK1) {
				tmove = from | (to<<8) | (bpawn<<16) | (board[to]<<20);
				*pmove++ = tmove | (bqueen<<24);
				*pmove++ = tmove | (bknight<<24);
				*pmove++ = tmove | (brook<<24);
				*pmove++ = tmove | (bbishop<<24);
			} else {
				*pmove++ = from | (to<<8) | (bpawn<<16) | (board[to]<<20);
			}
			bb &= NBB(to);                                              
		}
		bb = ((pawns_bb[black] & rank_mask[RANK2]) >> 8) & ~occupancy;
		while(bb) {														
            to = first_one(bb);        
			from = to + UU;
			*pmove++ = from | (to<<8) | (bpawn<<16) | (bqueen<<24);
			bb &= NBB(to);                                              
		}
		if(epsquare) {
			from = epsquare + RU;
			if(board[from] == bpawn)
				*pmove++ = from | (epsquare<<8) | (bpawn<<16) | (wpawn<<20) | EP_FLAG;
			
			from = epsquare + LU;
			if(board[from] == bpawn)
				*pmove++ = from | (epsquare<<8) | (bpawn<<16) | (wpawn<<20) | EP_FLAG;
		}
		/*knight*/
		current = plist[bknight];
		while(current) {
			from = current->sq;
			tmove = from | (bknight<<16);
			bb = knight_attacks(SQ8864(from)) & occupancyw;
			CAPS();
			current = current->next;
		}
		/*bishop*/
		current = plist[bbishop];
		while(current) {
			from = current->sq;
			tmove = from | (bbishop<<16);
			bb =  bishop_attacks(SQ8864(from),occupancy) & occupancyw;
			CAPS();
			current = current->next;
		}
		/*rook*/
		current = plist[brook];
		while(current) {
			from = current->sq;
			tmove = from | (brook<<16);
			bb =  rook_attacks(SQ8864(from),occupancy) & occupancyw;
			CAPS();
			current = current->next;
		}
		/*queen*/
		current = plist[bqueen];
		while(current) {
			from = current->sq;
			tmove = from | (bqueen<<16);
			bb =  queen_attacks(SQ8864(from),occupancy) & occupancyw;
			CAPS();
			current = current->next;
		}
		/*king*/
		from = plist[bking]->sq;
		tmove = from | (bking<<16);
		bb =  king_attacks(SQ8864(from)) & occupancyw;
		CAPS();
	}
	/*count*/
	pstack->count += int(pmove - spmove);
}
/*
Generate non captures
*/
#define NK_NONCAP(dir) {										\
		to = from + dir;										\
		if(board[to] == blank)									\
			*pmove++ = tmove | (to<<8);							\
}
#define BRQ_NONCAP(dir) {										\
	    to = from + dir;										\
		while(board[to] == blank) {								\
			*pmove++ = tmove | (to<<8);							\
			to += dir;											\
		}														\
}

void SEARCHER::gen_noncaps() {
	MOVE* pmove = &pstack->move_st[pstack->count],*spmove = pmove,tmove;
	int  from,to;
	PLIST current;
	
	if(player == white) {

		/*castling*/
		if((castle & WSLC_FLAG) && !attacks(black,E1)) {
			if(castle & WSC_FLAG &&
				board[F1] == blank &&
				board[G1] == blank &&
				!attacks(black,F1) &&
				!attacks(black,G1))
				*pmove++ = E1 | (G1<<8) | (wking<<16) | CASTLE_FLAG;
			if(castle & WLC_FLAG &&
				board[B1] == blank &&
				board[C1] == blank &&
				board[D1] == blank &&
				!attacks(black,C1) &&
				!attacks(black,D1)) {
				*pmove++ = E1 | (C1<<8) | (wking<<16) | CASTLE_FLAG;
			}
		}
		/*knight*/
		current = plist[wknight];
		while(current) {
			from = current->sq;
			tmove = from | (wknight<<16);
			NK_NONCAP(RRU);
			NK_NONCAP(LLD);
			NK_NONCAP(RUU);
			NK_NONCAP(LDD);
			NK_NONCAP(LLU);
			NK_NONCAP(RRD);
			NK_NONCAP(RDD);
			NK_NONCAP(LUU);
			current = current->next;
		}
		/*bishop*/
		current = plist[wbishop];
		while(current) {
			from = current->sq;
			tmove = from | (wbishop<<16);
			BRQ_NONCAP(RU);
			BRQ_NONCAP(LD);
			BRQ_NONCAP(LU);
			BRQ_NONCAP(RD);
			current = current->next;
		}
		/*rook*/
		current = plist[wrook];
		while(current) {
			from = current->sq;
			tmove = from | (wrook<<16);
			BRQ_NONCAP(UU);
			BRQ_NONCAP(DD);
			BRQ_NONCAP(RR);
			BRQ_NONCAP(LL);
			current = current->next;
		}
		/*queen*/
		current = plist[wqueen];
		while(current) {
			from = current->sq;
			tmove = from | (wqueen<<16);
			BRQ_NONCAP(RU);
			BRQ_NONCAP(LD);
			BRQ_NONCAP(LU);
			BRQ_NONCAP(RD);
			BRQ_NONCAP(UU);
			BRQ_NONCAP(DD);
			BRQ_NONCAP(RR);
			BRQ_NONCAP(LL);
			current = current->next;
		}
		/*king*/
		from = plist[wking]->sq;
		tmove = from | (wking<<16);
		NK_NONCAP(RU);
		NK_NONCAP(LD);
		NK_NONCAP(LU);
		NK_NONCAP(RD);
		NK_NONCAP(UU);
		NK_NONCAP(DD);
		NK_NONCAP(RR);
		NK_NONCAP(LL);
		
		/*pawn*/
		current = plist[wpawn];
		while(current) {
			from = current->sq;
			to = from + UU;
			if(board[to] == blank) {
				if(rank(to) == RANK8) {
					tmove = from | (to<<8) | (wpawn<<16);
					*pmove++ = tmove | (wknight<<24);
					*pmove++ = tmove | (wrook<<24);
					*pmove++ = tmove | (wbishop<<24);
				} else {
					*pmove++ = from | (to<<8) | (wpawn<<16);
					
					if(rank(from) == RANK2) {
						to += UU;
						if(board[to] == blank)
							*pmove++ = from | (to<<8) | (wpawn<<16);
					}
				}
			}	
			current = current->next;
		}
	} else {

		/*castling*/
		if((castle & BSLC_FLAG) && !attacks(white,E8)) {
			if(castle & BSC_FLAG &&
				board[F8] == blank &&
				board[G8] == blank &&
				!attacks(white,F8) &&
				!attacks(white,G8))
				*pmove++ = E8 | (G8<<8) | (bking<<16) | CASTLE_FLAG;
			if(castle & BLC_FLAG &&
				board[B8] == blank &&
				board[C8] == blank &&
				board[D8] == blank &&
				!attacks(white,C8) &&
				!attacks(white,D8)) {
				*pmove++ = E8 | (C8<<8) | (bking<<16) | CASTLE_FLAG;
			}
		}

		/*knight*/
		current = plist[bknight];
		while(current) {
			from = current->sq;
			tmove = from | (bknight<<16);
			NK_NONCAP(RRU);
			NK_NONCAP(LLD);
			NK_NONCAP(RUU);
			NK_NONCAP(LDD);
			NK_NONCAP(LLU);
			NK_NONCAP(RRD);
			NK_NONCAP(RDD);
			NK_NONCAP(LUU);
			current = current->next;
		}
		/*bishop*/
		current = plist[bbishop];
		while(current) {
			from = current->sq;
			tmove = from | (bbishop<<16);
			BRQ_NONCAP(RU);
			BRQ_NONCAP(LD);
			BRQ_NONCAP(LU);
			BRQ_NONCAP(RD);
			current = current->next;
		}
		/*rook*/
		current = plist[brook];
		while(current) {
			from = current->sq;
			tmove = from | (brook<<16);
			BRQ_NONCAP(UU);
			BRQ_NONCAP(DD);
			BRQ_NONCAP(RR);
			BRQ_NONCAP(LL);
			current = current->next;
		}
		/*queen*/
		current = plist[bqueen];
		while(current) {
			from = current->sq;
			tmove = from | (bqueen<<16);
			BRQ_NONCAP(RU);
			BRQ_NONCAP(LD);
			BRQ_NONCAP(LU);
			BRQ_NONCAP(RD);
			BRQ_NONCAP(UU);
			BRQ_NONCAP(DD);
			BRQ_NONCAP(RR);
			BRQ_NONCAP(LL);
			current = current->next;
		}
		
		/*king*/
		from = plist[bking]->sq;
		tmove = from | (bking<<16);
		NK_NONCAP(RU);
		NK_NONCAP(LD);
		NK_NONCAP(LU);
		NK_NONCAP(RD);
		NK_NONCAP(UU);
		NK_NONCAP(DD);
		NK_NONCAP(RR);
		NK_NONCAP(LL);
		
		/*pawn*/
		current = plist[bpawn];
		while(current) {
			from = current->sq;
			to = from + DD;
			if(board[to] == blank) {
				if(rank(to) == RANK1) {
					tmove = from | (to<<8) | (bpawn<<16);
					*pmove++ = tmove | (bknight<<24);
					*pmove++ = tmove | (brook<<24);
					*pmove++ = tmove | (bbishop<<24);
				} else {
					*pmove++ = from | (to<<8) | (bpawn<<16);
					
					if(rank(from) == RANK7) {
						to += DD;
						if(board[to] == blank)
							*pmove++ = from | (to<<8) | (bpawn<<16);
					}
				}
			}	
			current = current->next;
		}
	}
	/*count*/
	pstack->count += int(pmove - spmove);
}
/*
Generate check evastions
*/
#define K_EVADE(dir) {																					\
	    to = from + dir;																				\
        if(!(to & 0x88) && (check_dir1 != dir) && (check_dir2 != dir) && !attacks(opponent,to)) {		\
			if(board[to] == blank)																		\
			    *pmove++ = tmove | (to<<8);																\
		 	else if(PCOLOR(board[to]) == opponent)														\
				*pmove++ = tmove | (to<<8) | (board[to]<<20);											\
		}																								\
}

void SEARCHER::gen_evasions() {
	MOVE* pmove = &pstack->move_st[pstack->count],*spmove = pmove,tmove;
	int  from,to;
	PLIST current;
	int  sq,king_sq,att_sq,n_attackers,check_dir1,check_dir2;
	n_attackers = 0;
    check_dir1 = -2;
    check_dir2 = -2;
	king_sq = plist[COMBINE(player,king)]->sq;

	/*
	extract attack info
	1) Direct check
	2) Indirect check
	3) Direct + indirect check
	4) Indirect + indirect
	*/
	if(hstack[hply - 1].checks == 1) { 
		n_attackers = 1;
		att_sq = m_to(hstack[hply - 1].move);
		if(piece_mask[board[att_sq]] & QRBM)
			check_dir1 = sqatt_step(king_sq - att_sq);
	} else if(hstack[hply - 1].checks == 2) {
		n_attackers = 1;
		att_sq = hstack[hply - 1].rev_check;
		check_dir1 = sqatt_step(king_sq - att_sq);
	} else {
		n_attackers = 2;
		if(hstack[hply - 1].checks == 3) {
			att_sq = m_to(hstack[hply - 1].move);
			if(piece_mask[board[att_sq]] & QRBM)
				check_dir1 = sqatt_step(king_sq - att_sq);
			check_dir2 = sqatt_step(king_sq - hstack[hply - 1].rev_check);
		} else {
			att_sq = m_from(hstack[hply - 1].move);
			check_dir1 = sqatt_step(king_sq - att_sq);
			check_dir2 = sqatt_step(king_sq - hstack[hply - 1].rev_check);
		}
	}

	if(player == white) {
		//king moves
		from = king_sq;
		tmove = from | (wking<<16);
		K_EVADE(RU);
		K_EVADE(LD);
		K_EVADE(LU);
		K_EVADE(RD);
		K_EVADE(UU);
		K_EVADE(DD);
		K_EVADE(RR);
		K_EVADE(LL);
		
		if(n_attackers > 1) {
			pstack->count += int(pmove - spmove);
			return;
		}
		
		//pawn blocked
		if(check_dir1!=-2) {
			for(to = king_sq - check_dir1;to != att_sq;to -= check_dir1) {
				from = to - UU;
				if(board[from] == wpawn &&  !pinned_on_king(from,white)) {
					if(rank(to) == RANK8) {
						tmove = from | (to<<8) | (wpawn<<16);
						*pmove++ = tmove | (wqueen<<24);
						*pmove++ = tmove | (wknight<<24);
						*pmove++ = tmove | (wrook<<24);
						*pmove++ = tmove | (wbishop<<24);
					}
					else
						*pmove++ = from | (to<<8) | (wpawn<<16);
				} else if(board[from] == blank && rank(from) == RANK3) {
					from += DD;
					if(board[from] == wpawn && !pinned_on_king(from,white))
						*pmove++ = from | (to<<8) | (wpawn<<16);
				}
			}
		}
		
		//normal captures
		to = att_sq;
		
		from = to - RU;
		if(board[from] == wpawn && !pinned_on_king(from,white)) {
			if(rank(to) == RANK8) {
				tmove = from | (to<<8) | (wpawn<<16) | (board[to]<<20);
				*pmove++ = tmove | (wqueen<<24);
				*pmove++ = tmove | (wknight<<24);
				*pmove++ = tmove | (wrook<<24);
				*pmove++ = tmove | (wbishop<<24);
			}
			else 
				*pmove++ = from | (to<<8) | (wpawn<<16) | (board[to]<<20);
		}
		from = to - LU;
		if(board[from] == wpawn && !pinned_on_king(from,white)) {
			if(rank(to) == RANK8) {
				tmove = from | (to<<8) | (wpawn<<16) | (board[to]<<20);
				*pmove++ = tmove | (wqueen<<24);
				*pmove++ = tmove | (wknight<<24);
				*pmove++ = tmove | (wrook<<24);
				*pmove++ = tmove | (wbishop<<24);
			}
			else 
				*pmove++ = from | (to<<8) | (wpawn<<16) | (board[to]<<20);
		}
		
		
		//ep captures
		if(epsquare){
			from = epsquare - RU;
			sq = epsquare + DD;
			if(board[from] == wpawn &&
				(sq == att_sq || (sqatt_step(epsquare - king_sq) != 0 && sqatt_step(epsquare - king_sq) == -sqatt_step(epsquare-att_sq))) &&
				!pinned_on_king(sq,white) && !pinned_on_king(from,white))
				  *pmove++ = from | (epsquare<<8) | (wpawn<<16) | (bpawn<<20) | EP_FLAG;
			
			from = epsquare - LU;
			sq = epsquare + DD;
			if(board[from] == wpawn &&
				(sq == att_sq || (sqatt_step(epsquare - king_sq) != 0 && sqatt_step(epsquare - king_sq) == -sqatt_step(epsquare-att_sq))) &&
				!pinned_on_king(sq,white) && !pinned_on_king(from,white))
				  *pmove++ = from | (epsquare<<8) | (wpawn<<16) | (bpawn<<20) | EP_FLAG;
		}
		
		//knight
		current = plist[wknight];
		while(current) {
			from = current->sq;
			if(!pinned_on_king(from,white)){
				if(check_dir1!=-2) {
					for(to = king_sq - check_dir1;to != att_sq;to -= check_dir1) {
						if(sqatt_pieces(from - to) & NM)
							*pmove++ = from | (to<<8) | (wknight<<16);
					}
				}
				
				if(sqatt_pieces(from - att_sq) & NM)
					*pmove++ = from | (to<<8) | (wknight<<16) | (board[to]<<20);
			}
			current = current->next;
		}
		
		//bishop
		current = plist[wbishop];
		while(current) {
			from = current->sq;
			if(!pinned_on_king(from,white)){
				if(check_dir1!=-2) {
					for(to = king_sq - check_dir1;to != att_sq;to -= check_dir1) {
						if(sqatt_pieces(from - to) & BM)
							if(!blocked(from,to))
								*pmove++ = from | (to<<8) | (wbishop<<16);
					}
				}
				
				if(sqatt_pieces(from - att_sq) & BM)
					if(!blocked(from,att_sq))
						*pmove++ = from | (to<<8) | (wbishop<<16) | (board[to]<<20);
			}
			current = current->next;
		}
		
		//rook
		current = plist[wrook];
		while(current) {
			from = current->sq;
			if(!pinned_on_king(from,white)){
				if(check_dir1!=-2) {
					for(to = king_sq - check_dir1;to != att_sq;to -= check_dir1) {
						if(sqatt_pieces(from - to) & RM)
							if(!blocked(from,to))
								*pmove++ = from | (to<<8) | (wrook<<16);
					}
				}
				
				if(sqatt_pieces(from - att_sq) & RM)
					if(!blocked(from,att_sq))
						*pmove++ = from | (to<<8) | (wrook<<16) | (board[to]<<20);
			}
			current = current->next;
		}
		
		//queen
		current = plist[wqueen];
		while(current) {
			from = current->sq;
			if(!pinned_on_king(from,white)){
				if(check_dir1!=-2) {
					for(to = king_sq - check_dir1;to != att_sq;to -= check_dir1) {
						if(sqatt_pieces(from - to) & QM)
							if(!blocked(from,to))
								*pmove++ = from | (to<<8) | (wqueen<<16);
					}
				}
				
				if(sqatt_pieces(from - att_sq) & QM)
					if(!blocked(from,att_sq))
						*pmove++ = from | (to<<8) | (wqueen<<16) | (board[to]<<20);
			}
			current = current->next;
		}
	} else {
		//king moves
		from = king_sq;
		tmove = from | (bking<<16);
		K_EVADE(RU);
		K_EVADE(LD);
		K_EVADE(LU);
		K_EVADE(RD);
		K_EVADE(UU);
		K_EVADE(DD);
		K_EVADE(RR);
		K_EVADE(LL);
		
		if(n_attackers > 1) {
			pstack->count += int(pmove - spmove);
			return;
		}
		
		//pawn blocked
		if(check_dir1!=-2) {
			for(to = king_sq - check_dir1;to != att_sq;to -= check_dir1) {
				from = to - DD;
				if(board[from] == bpawn &&  !pinned_on_king(from,black)) {
					if(rank(to) == RANK1) {
						tmove = from | (to<<8) | (bpawn<<16);
						*pmove++ = tmove | (bqueen<<24);
						*pmove++ = tmove | (bknight<<24);
						*pmove++ = tmove | (brook<<24);
						*pmove++ = tmove | (bbishop<<24);
					}
					else
						*pmove++ = from | (to<<8) | (bpawn<<16);
				} else if(board[from] == blank && rank(from) == RANK6) {
					from += UU;
					if(board[from] == bpawn && !pinned_on_king(from,black))
						*pmove++ = from | (to<<8) | (bpawn<<16);
				}
			}
		}
		
		//normal captures
		to = att_sq;
		
		from = to - LD;
		if(board[from] == bpawn && !pinned_on_king(from,black)) {
			if(rank(to) == RANK1) {
				tmove = from | (to<<8) | (bpawn<<16) | (board[to]<<20);
				*pmove++ = tmove | (bqueen<<24);
				*pmove++ = tmove | (bknight<<24);
				*pmove++ = tmove | (brook<<24);
				*pmove++ = tmove | (bbishop<<24);
			}
			else 
				*pmove++ = from | (to<<8) | (bpawn<<16) | (board[to]<<20);
		}
		from = to - RD;
		if(board[from] == bpawn && !pinned_on_king(from,black)) {
			if(rank(to) == RANK1) {
				tmove = from | (to<<8) | (bpawn<<16) | (board[to]<<20);
				*pmove++ = tmove | (bqueen<<24);
				*pmove++ = tmove | (bknight<<24);
				*pmove++ = tmove | (brook<<24);
				*pmove++ = tmove | (bbishop<<24);
			}
			else 
				*pmove++ = from | (to<<8) | (bpawn<<16) | (board[to]<<20);
		}
		
		
		//ep captures
		if(epsquare){
			from = epsquare - LD;
			sq = epsquare + UU;
			if(board[from] == bpawn &&
				(sq == att_sq || (sqatt_step(epsquare - king_sq) != 0 && sqatt_step(epsquare - king_sq) == -sqatt_step(epsquare - att_sq))) &&
				!pinned_on_king(sq,black) && !pinned_on_king(from,black))
				*pmove++ = from | (epsquare<<8) | (bpawn<<16) | (wpawn<<20) | EP_FLAG;
			
			from = epsquare - RD;
			sq = epsquare + UU;
			if(board[from] == bpawn &&
				(sq == att_sq || (sqatt_step(epsquare - king_sq) != 0 && sqatt_step(epsquare - king_sq) == -sqatt_step(epsquare - att_sq))) &&
				!pinned_on_king(sq,black) && !pinned_on_king(from,black))
				*pmove++ = from | (epsquare<<8) | (bpawn<<16) | (wpawn<<20) | EP_FLAG;
		}
		
		//knight
		current = plist[bknight];
		while(current) {
			from = current->sq;
			if(!pinned_on_king(from,black)){
				if(check_dir1!=-2) {
					for(to = king_sq - check_dir1;to != att_sq;to -= check_dir1) {
						if(sqatt_pieces(from - to) & NM)
							*pmove++ = from | (to<<8) | (bknight<<16);
					}
				}
				
				if(sqatt_pieces(from - att_sq) & NM)
					*pmove++ = from | (to<<8) | (bknight<<16) | (board[to]<<20);
			}
			current = current->next;
		}
		
		//bishop
		current = plist[bbishop];
		while(current) {
			from = current->sq;
			if(!pinned_on_king(from,black)){
				if(check_dir1!=-2) {
					for(to = king_sq - check_dir1;to != att_sq;to -= check_dir1) {
						if(sqatt_pieces(from - to) & BM)
							if(!blocked(from,to))
								*pmove++ = from | (to<<8) | (bbishop<<16);
					}
				}
				
				if(sqatt_pieces(from - att_sq) & BM)
					if(!blocked(from,att_sq))
						*pmove++ = from | (to<<8) | (bbishop<<16) | (board[to]<<20);
			}
			current = current->next;
		}
		
		//rook
		current = plist[brook];
		while(current) {
			from = current->sq;
			if(!pinned_on_king(from,black)){
				if(check_dir1!=-2) {
					for(to = king_sq - check_dir1;to != att_sq;to -= check_dir1) {
						if(sqatt_pieces(from - to) & RM)
							if(!blocked(from,to))
								*pmove++ = from | (to<<8) | (brook<<16);
					}
				}
				
				if(sqatt_pieces(from - att_sq) & RM)
					if(!blocked(from,att_sq))
						*pmove++ = from | (to<<8) | (brook<<16) | (board[to]<<20);
			}
			current = current->next;
		}
		
		//queen
		current = plist[bqueen];
		while(current) {
			from = current->sq;
			if(!pinned_on_king(from,black)){
				if(check_dir1!=-2) {
					for(to = king_sq - check_dir1;to != att_sq;to -= check_dir1) {
						if(sqatt_pieces(from - to) & QM)
							if(!blocked(from,to))
								*pmove++ = from | (to<<8) | (bqueen<<16);
					}
				}
				
				if(sqatt_pieces(from - att_sq) & QM)
					if(!blocked(from,att_sq))
						*pmove++ = from | (to<<8) | (bqueen<<16) | (board[to]<<20);
			}
			current = current->next;
		}
	}
	/*count*/
	pstack->count += int(pmove - spmove);
}

void SEARCHER::gen_all() {
	gen_caps();
	gen_noncaps();
}
/*
Generate check moves
*/
#define N_CHK(dir) {																	\
	    to = from + dir;																\
		if(pinned || (sqatt_pieces(to - oking_sq) & NM)) {								\
            if(board[to] == blank)														\
			    *pmove++ = tmove | (to<<8);												\
		}																				\
}
#define K_CHK(dir) {																	\
		if(ABS(pinned) != ABS(dir)) {													\
			to = from + dir;															\
            if(board[to] == blank)														\
			    *pmove++ = tmove | (to<<8);												\
		}																				\
}
#define BRQ_CHK(moving,dir) {															\
	    to = from + dir;																\
        while(board[to] == blank) {														\
			if(pinned || ((sqatt_pieces(to - oking_sq) & piece_mask[moving])			\
							&& !blocked(to,oking_sq)))									\
			   *pmove++ = tmove | (to<<8);												\
            to += dir;																	\
		}																				\
}

void SEARCHER::gen_checks(){
	MOVE* pmove = &pstack->move_st[pstack->count],*spmove = pmove,tmove;
	int   from,to,pinned;
	PLIST current;
	int oking_sq = plist[COMBINE(opponent,king)]->sq;
	
	if(player == white) {
		//direct pawn checks
		to = oking_sq + RD;
		from = to + DD;
		if(board[to] == blank) {
			if(board[from] == wpawn)
				*pmove++ = from | (to<<8) | (wpawn<<16);
			else if(board[from] == blank && rank(from) == RANK3) {
				from += DD;
				if(board[from] == wpawn)
					*pmove++ = from | (to<<8) | (wpawn<<16);
			}
		}
		to = oking_sq + LD;
		from = to + DD;
		if(board[to] == blank) {
			if(board[from] == wpawn)
				*pmove++ = from | (to<<8) | (wpawn<<16);
			else if(board[from] == blank && rank(from) == RANK3) {
				from += DD;
				if(board[from] == wpawn)
					*pmove++ = from | (to<<8) | (wpawn<<16);
			}
		}
		
		//pawn indirect checks
		current = plist[wpawn];
		while(current) {
			from = current->sq;
			pinned = pinned_on_king(from,black);
			if(pinned && pinned != DD) {
				to = from + UU;
				if(board[to] == blank) {
					if(rank(to) != RANK8 && rank(to) != RANK1) {
						if(pinned || (sqatt_pieces(to - oking_sq) & WPM))
							*pmove++ = from | (to<<8) | (wpawn<<16);
						if(rank(from)==RANK2) {
							to += UU;
							if(board[to] == blank)
								*pmove++ = from | (to<<8) | (wpawn<<16);
						}
					}
				}	
			}
			current = current->next;
		}
		//knight
		current = plist[wknight];
		while(current) {
			from = current->sq;
			tmove = from | (wknight<<16);
			pinned = pinned_on_king(from,black);
			N_CHK(RRU);
			N_CHK(LLD);
			N_CHK(RUU);
			N_CHK(LDD);
			N_CHK(LLU);
			N_CHK(RRD);
			N_CHK(RDD);
			N_CHK(LUU);
			current = current->next;
		}
		//bishop
		current = plist[wbishop];
		while(current) {
			from = current->sq;
			tmove = from | (wbishop<<16);
			pinned = pinned_on_king(from,black);
			BRQ_CHK(bishop,RU);
			BRQ_CHK(bishop,LD);
			BRQ_CHK(bishop,LU);
			BRQ_CHK(bishop,RD);
			current = current->next;
		}
		
		//rook
		current = plist[wrook];
		while(current) {
			from = current->sq;
			tmove = from | (wrook<<16);
			pinned = pinned_on_king(from,black);
			BRQ_CHK(rook,UU);
			BRQ_CHK(rook,DD);
			BRQ_CHK(rook,RR);
			BRQ_CHK(rook,LL);
			current = current->next;
		}
		
		//queen
		current = plist[wqueen];
		while(current) {
			from = current->sq;
			tmove = from | (wqueen<<16);
			pinned = pinned_on_king(from,black);
			BRQ_CHK(queen,RU);
			BRQ_CHK(queen,LD);
			BRQ_CHK(queen,LU);
			BRQ_CHK(queen,RD);
			BRQ_CHK(queen,UU);
			BRQ_CHK(queen,DD);
			BRQ_CHK(queen,RR);
			BRQ_CHK(queen,LL);
			current = current->next;
		}
		//king
		from = plist[wking]->sq;
		tmove = from | (wking<<16);
		pinned = pinned_on_king(from,black);
		if(pinned) {
			K_CHK(RU);
			K_CHK(LD);
			K_CHK(LU);
			K_CHK(RD);
			K_CHK(UU);
			K_CHK(DD);
			K_CHK(RR);
			K_CHK(LL);
		}
	} else {
		//direct pawn checks
		to = oking_sq + LU;
		from = to + UU;
		if(board[to] == blank) {
			if(board[from] == bpawn)
				*pmove++ = from | (to<<8) | (bpawn<<16);
			else if(board[from] == blank && rank(from) == RANK6) {
				from += UU;
				if(board[from] == bpawn)
					*pmove++ = from | (to<<8) | (bpawn<<16);
			}
		}
		to = oking_sq + LD;
		from = to + DD;
		if(board[to] == blank) {
			if(board[from] == bpawn)
				*pmove++ = from | (to<<8) | (bpawn<<16);
			else if(board[from] == blank && rank(from) == RANK6) {
				from += UU;
				if(board[from] == bpawn)
					*pmove++ = from | (to<<8) | (bpawn<<16);
			}
		}
		
		//pawn indirect checks
		current = plist[bpawn];
		while(current) {
			from = current->sq;
			pinned = pinned_on_king(from,white);
			if(pinned && pinned != UU) {
				to = from + DD;
				if(board[to] == blank) {
					if(rank(to) != RANK1 && rank(to) != RANK8) {
						if(pinned || (sqatt_pieces(to - oking_sq) & BPM))
							*pmove++ = from | (to<<8) | (bpawn<<16);
						if(rank(from) == RANK7) {
							to += DD;
							if(board[to] == blank)
								*pmove++ = from | (to<<8) | (bpawn<<16);
						}
					}
				}	
			}
			current = current->next;
		}
		//knight
		current = plist[bknight];
		while(current) {
			from = current->sq;
			tmove = from | (bknight<<16);
			pinned = pinned_on_king(from,white);
			N_CHK(RRU);
			N_CHK(LLD);
			N_CHK(RUU);
			N_CHK(LDD);
			N_CHK(LLU);
			N_CHK(RRD);
			N_CHK(RDD);
			N_CHK(LUU);
			current = current->next;
		}
		//bishop
		current = plist[bbishop];
		while(current) {
			from = current->sq;
			tmove = from | (bbishop<<16);
			pinned = pinned_on_king(from,white);
			BRQ_CHK(bishop,RU);
			BRQ_CHK(bishop,LD);
			BRQ_CHK(bishop,LU);
			BRQ_CHK(bishop,RD);
			current = current->next;
		}
		
		//rook
		current = plist[brook];
		while(current) {
			from = current->sq;
			tmove = from | (brook<<16);
			pinned = pinned_on_king(from,white);
			BRQ_CHK(rook,UU);
			BRQ_CHK(rook,DD);
			BRQ_CHK(rook,RR);
			BRQ_CHK(rook,LL);
			current = current->next;
		}
		
		//queen
		current = plist[bqueen];
		while(current) {
			from = current->sq;
			tmove = from | (bqueen<<16);
			pinned = pinned_on_king(from,white);
			BRQ_CHK(queen,RU);
			BRQ_CHK(queen,LD);
			BRQ_CHK(queen,LU);
			BRQ_CHK(queen,RD);
			BRQ_CHK(queen,UU);
			BRQ_CHK(queen,DD);
			BRQ_CHK(queen,RR);
			BRQ_CHK(queen,LL);
			current = current->next;
		}
		
		
		//king moves
		from = plist[bking]->sq;
		tmove = from | (bking<<16);
		pinned = pinned_on_king(from,white);
		if(pinned) {
			K_CHK(RU);
			K_CHK(LD);
			K_CHK(LU);
			K_CHK(RD);
			K_CHK(UU);
			K_CHK(DD);
			K_CHK(RR);
			K_CHK(LL);
		}
	}

	/*count*/
	pstack->count += int(pmove - spmove);
}
/*
incremental move generator
*/

#define HISTORY(move) (history[m_piece(move)][SQ8864(m_to(move))])
#define REFUTE(move) (refutation[m_piece(move)][SQ8864(m_to(move))])

MOVE SEARCHER::get_move() {
	register MOVE move;
	register int i,start;
	register int* pscore;

	/*initialization*/
	if(pstack->gen_status == GEN_START) {
		pstack->current_index = 0;
		pstack->gen_status = GEN_HASHM;
		pstack->bad_index = 0;
		pstack->count = 0;
	} else if(pstack->gen_status == GEN_RESET) {
		pstack->current_index = 0;
		pstack->gen_status = GEN_AVAIL;
		pstack->legal_moves = 0;
		pstack->sortm = 0;
	} else if(pstack->gen_status == GEN_RESET_SORT) {
		pstack->current_index = 0;
		pstack->gen_status = GEN_AVAIL;
		pstack->legal_moves = 0;
		pstack->sortm = 1;
	}

DO_AGAIN:

	/*moves are already generated*/
	if(pstack->gen_status == GEN_AVAIL) {
		if(pstack->current_index >= pstack->count) {
			pstack->gen_status = GEN_END;
			return 0;
		}
		goto END;
	}

	while(pstack->current_index >= pstack->count 
		&& pstack->gen_status <= GEN_LOSCAPS) {
        
		if(pstack->gen_status == GEN_HASHM) {
			if(hply >= 1 && hstack[hply - 1].checks) {
				gen_evasions();
				pstack->sortm = 1;
				for(i = 0; i < pstack->count;i++) {
					move = pstack->move_st[i];
					pscore = &pstack->score_st[i];
					
					if(move == pstack->hash_move)
						*pscore = 10000;
					else if(move == pstack->killer[0])
						*pscore = 90;
					else if(move == pstack->killer[1])
						*pscore = 80;
					else if(is_cap_prom(move)) {
						*pscore = see(move);
						if(*pscore < 0) *pscore -= MAX_HIST;
					} else {
						*pscore = HISTORY(move) - MAX_HIST;
					}
				}
				pstack->gen_status = GEN_END;
			} else {
				pstack->sortm = 0;
				if(pstack->hash_move) {
					pstack->score_st[pstack->count] = 10000;
					pstack->move_st[pstack->count++] = pstack->hash_move;
				}
			}
		} else if(pstack->gen_status == GEN_CAPS) {
			start = pstack->count;
			gen_caps();
			pstack->sortm = 1;
			for(i = start; i < pstack->count;i++) {
				move = pstack->move_st[i];
				if(move == pstack->hash_move)
					pstack->score_st[i] = -MAX_NUMBER;
				else {
				    pstack->score_st[i] =   32 * piece_see_v[m_promote(move)] 
					                      + 16 * piece_see_v[m_capture(move)]
										  - piece_see_v[m_piece(move)];
				}
			}
        } else if(pstack->gen_status == GEN_KILLERS) {
			pstack->sortm = 0;
			move = pstack->killer[0];
			if(move 
				&& move != pstack->hash_move
				&& is_legal_fast(move)
				) {
				pstack->score_st[pstack->count] = 2000;
				pstack->move_st[pstack->count++] = move;
			}
			move = pstack->killer[1];
			if(move 
				&& move != pstack->hash_move
				&& is_legal_fast(move)
				) {
				pstack->score_st[pstack->count] = 1000;
				pstack->move_st[pstack->count++] = move;
			}
			pstack->killer[2] = 0;
			if(hply >= 1) {
				move = hstack[hply - 1].move;
				move = REFUTE(move);
				if(move 
					&& move != pstack->hash_move
					&& move != pstack->killer[0]
				    && move != pstack->killer[1]
					&& is_legal_fast(move)
					) {
						pstack->score_st[pstack->count] = 900;
						pstack->move_st[pstack->count++] = move;
						pstack->killer[2] = move;
				}
			}
		} else if(pstack->gen_status == GEN_NONCAPS) {
			start = pstack->count;
			pstack->noncap_start = start;
			gen_noncaps();
			pstack->sortm = 2;
			for(i = start; i < pstack->count;i++) {
				move = pstack->move_st[i];
				if(move == pstack->hash_move
					|| (move == pstack->killer[0]) 
					|| (move == pstack->killer[1]) 
					|| (move == pstack->killer[2]) 
					) {
						pstack->score_st[i] = -MAX_NUMBER;
				} else {
					if(is_castle(move)) {
						pstack->score_st[i] = 1000;
					} else {
						pstack->score_st[i] = HISTORY(move) - MAX_HIST + 
							400 + 2 * (pcsq[m_piece(move)][m_to(move)] - pcsq[m_piece(move)][m_from(move)]);
					}
				}
			}
		} else if(pstack->gen_status == GEN_LOSCAPS) {
			pstack->sortm = 0;
			if(pstack->bad_index) { 
				for(i = 0; i < pstack->bad_index; i++) {
					pstack->move_st[pstack->count] = pstack->bad_st[i];
					pstack->score_st[pstack->count] = 0;
					pstack->count++;
				}
			}
		}
		pstack->gen_status++;
	}
	
	if(pstack->current_index >= pstack->count)
		return 0;
	
END:
	if(pstack->sortm)
        pstack->sort(pstack->current_index,pstack->count);  

	if(pstack->score_st[pstack->current_index] == -MAX_NUMBER) {
		pstack->current_index++;
		goto DO_AGAIN;
	}

	if(hply >= 1 && hstack[hply - 1].checks) {
	} else {
		move = pstack->move_st[pstack->current_index];

		if(in_check(move)) {
			pstack->current_index++;
			goto DO_AGAIN;
		}

		if(pstack->gen_status - 1 == GEN_CAPS) {
			if(piece_see_v[m_capture(move)] >= piece_see_v[m_piece(move)]);
			else {
				pstack->score_st[pstack->current_index] = see(move);
				if(pstack->score_st[pstack->current_index] < 0) {
					pstack->bad_st[pstack->bad_index] = move;
					pstack->bad_index++;
					pstack->current_index++;
					goto DO_AGAIN;
				}
			}
		}
	}

	pstack->current_move = pstack->move_st[pstack->current_index];
	pstack->current_index++;
	return pstack->current_move;
}
/*
qsearch move generator
*/
MOVE SEARCHER::get_qmove() {
    MOVE move;
	int* pscore;

	if(pstack->gen_status == GEN_START) {
		pstack->current_index = 0;
		pstack->count = 0;
		pstack->gen_status = GEN_CAPS;
	}

DO_AGAIN:

	while(pstack->current_index >= pstack->count 
		&& pstack->gen_status <= GEN_QNONCAPS) {
		
		if(pstack->gen_status == GEN_CAPS) {
			if(hply >= 1 && hstack[hply - 1].checks) {
				gen_evasions();
			    pstack->sortm = 1;
				for(int i = 0; i < pstack->count;i++) {
					move = pstack->move_st[i];
					pscore = &pstack->score_st[i];
					if(is_cap_prom(move)) {
						*pscore = see(move);
						if(*pscore < 0) *pscore -= MAX_HIST;
					} else {
						*pscore = HISTORY(move) - MAX_HIST;
					}
				}
				pstack->gen_status = GEN_END;
			} else {
				const bool recap = (hply >= 1 && pstack->depth <= -4 * UNITDEPTH);
			    gen_caps(recap);
				pstack->sortm = 1;
				for(int i = 0; i < pstack->count;i++) {
					move = pstack->move_st[i];
					pstack->score_st[i] =   32 * piece_see_v[m_promote(move)] 
					                      + 16 * piece_see_v[m_capture(move)]
										  - piece_see_v[m_piece(move)];
				}
			}
		} else if(pstack->gen_status == GEN_QNONCAPS) {
			if(pstack->qcheck_depth > 0) {  
				gen_checks();
				pstack->sortm = 0;
			}
		}
		pstack->gen_status++;
	}
	
	if(pstack->current_index >= pstack->count)
		return 0;

	if(pstack->sortm)
		pstack->sort(pstack->current_index,pstack->count);

	if(hply >= 1 && hstack[hply - 1].checks) {
	} else {
		move = pstack->move_st[pstack->current_index];
		if(in_check(move)) {
			pstack->current_index++;
			goto DO_AGAIN;
		}
		if(pstack->gen_status - 1 == GEN_CAPS
			&& piece_see_v[m_capture(move)] >= piece_see_v[m_piece(move)]);
		else {
			if(see(move) < 0) {
				pstack->current_index++;
				goto DO_AGAIN;
			}
		}
	}

	pstack->current_move = pstack->move_st[pstack->current_index];
	pstack->current_index++;
	return pstack->current_move;
}
/*
* History and killers
*/
void SEARCHER::update_history(MOVE move) {
	register int i,j,temp;
	temp = (pstack->depth);
	temp = (HISTORY(move) += (temp * temp));
	if(temp >= MAX_HIST) {
		for(i = 0;i < 14;i++)
			for(j = 0;j < 64;j++)
				history[i][j] >>= 1;
	}
	if(move != pstack->killer[0]) {
		pstack->killer[1] = pstack->killer[0];
		pstack->killer[0] = move;
	}
	MOVE cMove;
	if(hply >= 1 && (cMove = hstack[hply - 1].move) 
		&& pstack->depth > UNITDEPTH) {
		REFUTE(cMove) = move;
	}
}
void SEARCHER::clear_history() {
	memset(history,0,sizeof(history));
	memset(refutation,0,sizeof(refutation));
	for(int i = 0;i < MAX_PLY;i++) {
		stack[i].killer[0] = stack[i].killer[1] = 0;
	}
}

#undef HISTORY
#undef REFUTE

