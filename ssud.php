#!/usr/bin/env php
<?php

const NOINDEX = 82;

$grid = read_grid();
if (solve_grid($grid)) {
  print_grid($grid);
} else {
  echo "no solution\n";
}

exit(0);

/* -------------------------------------------- */

function read_grid(): array
{
  $grid = array_fill(0, 81, 0);
  $rows = [];
  $cols = [];
  $grps = [];
  $col  = 0;
  $row  = 0;
  for ($idx = 0; $idx < 81; ++$idx) {
    $chr = fgetc(STDIN);
    if ($chr === false) {
      echo 'premature end of input in row ',
      $row + 1, ' and column ', $col + 1, PHP_EOL;
      exit(1);
    }
    if ($chr != ' ') {
      $ord = ord($chr);
      if ($ord < ord('1') || $ord > ord('9')) {
        echo 'invalid value `', $chr, '` (', $ord, ') in row',
        $row + 1, ' and column ', $col + 1, PHP_EOL;
        exit(1);
      }
      $val = $ord - ord('0');
      $off = $val - 1;
      if (isset($rows[$row][$off])) {
        echo 'duplicate value ', $val, ' in row ',
        $row + 1, '(column ', $col + 1, ') - value ',
        'already seen in column ', $rows[$row][$off] + 1;
        exit(1);
      }
      if (isset($cols[$col][$off])) {
        echo 'duplicate value ', $val, ' in column ',
        $col + 1, '(row ', $row + 1, ') - value',
        'already seen in row ', $cols[$col][$off] + 1;
        exit(1);
      }
      $grp = (($row / 3) | 0) * 3 + (($col / 3) | 0);
      if (isset($grps[$grp][$off])) {
        echo 'duplicate value ', $val, ' in group ',
        $grp + 1, ' (row ', $row + 1, ' and column ',
        $col + 1, ')';
        exit(1);
      }
      $grid[$idx]       = $val;
      $rows[$row][$off] = $col;
      $cols[$col][$off] = $row;
      $grps[$grp][$off] = true;
    }
    if ($col++ == 8) {
      $chr = fgetc(STDIN);
      if ($chr != "\n") {
        echo 'unexpected input `', $chr, '` (',
        ord($chr), ') at index ', $idx;
        exit(1);
      }
      $col = 0;
      $row += 1;
    }
  }
  return $grid;
}

function calc_score(array $grid, int $idx): int
{
  $seen = array_fill(0, 10, 0);
  $row  = ($idx / 9) | 0;
  $col  = ($idx % 9) | 0;
  $rx   = (($row / 3) | 0) * 3;
  $ry   = (($col / 3) | 0) * 3;
  for ($i = 0; $i < 9; ++$i) {
    $rnm = $grid[(($row * 9) | 0) + $i];
    $cnm = $grid[$col + (($i * 9) | 0)];
    $reg =
      ($rx + (($i / 3) | 0)) * 9 +
      ($ry + (($i % 3) | 0));
    $gnm        = $grid[$reg];
    $seen[$rnm] = 1;
    $seen[$cnm] = 1;
    $seen[$gnm] = 1;
  }
  $scr = 0;
  for ($i = 1; $i < 10; ++$i) {
    $scr += $seen[$i];
  }
  return $scr;
}

function find_slot(array $grid): int
{
  $idx = NOINDEX;
  $csc = 0;
  $psc = 0;
  for ($i = 0; $i < 81; ++$i) {
    if ($grid[$i] === 0) {
      $csc = calc_score($grid, $i);
      if ($csc > 7) {
        return $i;
      }
      if ($csc > $psc) {
        $psc = $csc;
        $idx = $i;
      }
    }
  }
  return $idx;
}

function solve_grid(array &$grid): bool
{
  $idx = find_slot($grid);
  if ($idx === NOINDEX) {
    /* no slot found */
    return true;
  }
  for ($num = 1; $num <= 9; ++$num) {
    if (check_num($grid, $idx, $num)) {
      $grid[$idx] = $num;
      if (solve_grid($grid)) {
        return true;
      }
      $grid[$idx] = 0;
    }
  }
  /* no solution found */
  return false;
}

function check_num(array $grid, int $idx, int $num): bool
{
  $row = ($idx / 9) | 0;
  $col = ($idx % 9) | 0;
  $rx  = (($row / 3) | 0) * 3;
  $ry  = (($col / 3) | 0) * 3;
  for ($i = 0; $i < 9; ++$i) {
    if (
      $num === $grid[($row * 9) + $i] ||
      $num === $grid[$col + ($i * 9)]
    ) {
      return false;
    }
    $reg =
      ($rx + (($i / 3) | 0)) * 9 +
      ($ry + (($i % 3) | 0));
    if ($num === $grid[$reg]) {
      return false;
    }
  }
  return true;
}

function print_grid(array $grid)
{
  $col = 0;
  for ($i = 0; $i < 81; ++$i) {
    echo $grid[$i];
    if ($col++ > 7) {
      echo PHP_EOL;
      $col = 0;
    }
  }
  echo PHP_EOL;
}
