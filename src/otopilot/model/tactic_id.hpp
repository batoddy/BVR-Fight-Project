#pragma once

// Uçağın şu an uyguladığı taktik. Manevra (Phase) taktiğin o anki adımıdır.
// Sayı değerleri sabit kalmalı: ileride CSV'ye ve görüntüleyiciye sayı olarak gidecek. Yeni taktik sona eklenir.
enum class TacticId
{
    Commit = 0,         // düşmana uç
    LongSkate = 1,      // erken atış, crank, erken out
    Skate = 2,          // atış, crank, DOR'da out
    ShortSkate = 3,     // atış, crank, MAR'da out
    Banzai = 4,         // atış, beam, kilit varsa out, yoksa takip
    Recommit = 5,       // atış açısına dön
    Abort = 6,          // out, düşman arkada kalınca biter
    FlankAttack = 7,    // yan açı kazan, sonra atış
    BeamDefense = 8,    // düşmana dik uç
    NotchDefense = 9,   // füzeye dik uç
    DragDefense = 10,   // füzeye sırt dön
    GuideEvade = 11,    // füzeyi destekle, tehdit yaklaşınca kaç
};
