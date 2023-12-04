// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
//
// diff - computes a shortest edit script (SES) given two sequences
//
// Prototype
//
//   template<class equal_t, class edit_t>
//   ptrdiff_t diff(size_t aoff, size_t n, size_t boff, size_t m, const
//       equal_t& equal, const edit_t &edit, const int & cancel = 0, ptrdiff_t
//       dmax = INT_MAX);
//
//   template<class equal_t, class edit_t>
//   ptrdiff_t diff(size_t aoff, size_t n, size_t boff, size_t m, const
//       equal_t& equal, const edit_t &edit, const int & cancel, ptrdiff_t dmax,
//       std::vector<ptrdiff_t> &buf);
//
// Description
//
//   Computes length of the shortest edit script of two sequences with lengths
//   `n', `m' starting at offsets `aoff', `boff'.
//
//   Let `size_t j, i' be position in the compared sequences then `equal(i, j)'
//   tells whether the symbol at position `i' in the first sequence equals the
//   symbol at position `j' in the seccond sequence.
//
//   The shortest edit script itself is reported by calling `edit(op, off,
//   len)', where `op' identifies the edit operation (could be `diff_base::
//   ed_match', `diff_base::ed_delete' or `diff_base::ed_insert'), `off' and
//   `len' indicate the offset and length of the subsequence that matches or
//   should be deleted from the first sequence a or inserted to the seccond
//   sequence.  The shortest edit script is reported from the left to the
//   right, i.e.  `off' in subsequent call to `edit' is never smaller than in
//   the previous call.
//
//   Whenever value of `cancel' becomes false, computation is terminated by
//   throwing a C++ exception of type `diff_exception'.
//
//   `dmax' is maximal length of the shortest edit script. If the shortest edit
//   script is greater than `dmax' then `diff' fails and returns `dmax'.
//
//   `buf' is temporary buffer used for computations. If not supplied, it is
//   created temporarily for each call to `diff'.
//
//
// ****************************************************************************
//
// Some helper classes are defined in this header. See below.
//
// null_edit -- accepts op, off, len and does nothing, useful when you need
// only the length of the shortest edit script
//
// ed_script_builder -- builds an edit script
//
// string_comparator -- binary predicate, tells whether two strings equals at
// specified indicies
//
//
// ****************************************************************************
//
// Ported to C++ by Lukas Cerman <lukas.cerman altap.cz> in January 2006.
//
// Original copyright notice:
//
// Copyright (c) 2004 Michael B. Allen <mba2000 ioplex.com>
//
// The MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

// This algorithm is basically Myers' solution to SES/LCS with the Hirschberg
// linear space refinement as described in the following publication:
//
//   E. Myers, ``An O(ND) Difference Algorithm and Its Variations,''
//   Algorithmica 1, 2 (1986), 251-266.
//   http://www.cs.arizona.edu/people/gene/PAPERS/diff.ps
//
// This is the same algorithm used by GNU diff(1).
//

// ****************************************************************************
//
// null_edit -- accepts op, off, len and does nothing, useful when you need
// only shortest edit script length
//

class null_edit
{
public:
    void operator()(char op, size_t off, size_t len) {}
};

// ****************************************************************************
//
// ed_script_builder -- builds an edit script
//

class ed_script_builder
{
public:
    struct edit
    {
        char op;    // diff_base::ed_match, ed_delete, ed_insert
        size_t off; // off into s1 if MATCH or DELETE but s2 if INSERT
        size_t len;
        edit(char op, size_t off, size_t len) : op(op), off(off), len(len) {}
    };
    typedef std::vector<edit> ed_script;
    typedef std::vector<edit>::iterator ed_iterator;

    ed_script_builder(ed_script& ses) : ses(ses) {}

    void operator()(char op, size_t off, size_t len)
    {
        if (len == 0)
            return;

        // Add an edit to the SES (or coalesce if the op is the same)
        if (ses.empty() || ses.back().op != op)
            ses.push_back(edit(op, off, len));
        else
            ses.back().len += len;
    }

protected:
    ed_script& ses;
};

// ****************************************************************************
//
//  sequence_comparator -- binary predicate, tells whether two random access
//  sequences equal at specified indicies, sequences are represented by random
//  access iterators to their beginning
//

template <class Arg1, class Arg2, class Result>
struct binary_function
{
    using first_argument_type = Arg1;
    using second_argument_type = Arg2;
    using result_type = Result;
};

template <class iterator>
class sequence_comparator_t : binary_function<size_t, size_t, bool>
{
public:
    sequence_comparator_t(const iterator& a, const iterator& b) : a(a), b(b) {}
    bool operator()(size_t i, size_t j) const { return a[i] == b[j]; }

protected:
    const iterator a;
    const iterator b;
};

template <class iterator>
sequence_comparator_t<iterator>
sequence_comparator(const iterator a, const iterator b)
{
    return sequence_comparator_t<iterator>(a, b);
}

// ****************************************************************************
//
// diff_exception
//

class diff_exception : public std::exception
{
public:
    diff_exception() : exception("") {}
};

// ****************************************************************************
//
// diff
//

class diff_base
{
public:
    enum
    {
        ed_match,
        ed_delete,
        ed_insert
    };

protected:
    struct middle_snake
    {
        ptrdiff_t x, y, u, v;
    };

    ptrdiff_t dmax;
    std::vector<ptrdiff_t>& buf;
    std::vector<ptrdiff_t>::iterator fvi;
    std::vector<ptrdiff_t>::iterator rvi;
    const int& cancel;

    diff_base(const int& cancel, ptrdiff_t dmax, std::vector<ptrdiff_t>& buf)
        : dmax(dmax), buf(buf), cancel(cancel) {}

    void setfv(ptrdiff_t k, ptrdiff_t val) { fvi[k] = val; }
    void setrv(ptrdiff_t k, ptrdiff_t val) { rvi[k] = val; }
    ptrdiff_t fv(ptrdiff_t k) { return fvi[k]; }
    ptrdiff_t rv(ptrdiff_t k) { return rvi[k]; }
};

template <class equal_t, class edit_t>
class diff_t : public diff_base
{
public:
    diff_t(const equal_t& equal, const edit_t& edit, const int& cancel,
           ptrdiff_t dmax, std::vector<ptrdiff_t>& buf)
        : diff_base(cancel, dmax, buf), equal(equal), edit(edit) {}

    ptrdiff_t operator()(size_t aoff, size_t n, size_t boff, size_t m);

protected:
    equal_t equal;
    edit_t edit;

    ptrdiff_t find_middle_snake(size_t aoff, size_t n, size_t boff, size_t m,
                                middle_snake& ms);
    ptrdiff_t ses(size_t aoff, size_t n, size_t boff, size_t m);
};

template <class equal_t, class edit_t>
ptrdiff_t
diff(size_t aoff, size_t n, size_t boff, size_t m, const equal_t& equal,
     const edit_t& edit, const int& cancel, ptrdiff_t dmax,
     std::vector<ptrdiff_t>& buf)
{
    return diff_t<equal_t, edit_t>(equal, edit, cancel, dmax, buf)(aoff, n, boff, m);
}

template <class equal_t, class edit_t>
ptrdiff_t
diff(size_t aoff, size_t n, size_t boff, size_t m, const equal_t& equal,
     const edit_t& edit, const int& cancel = 0, ptrdiff_t dmax = INT_MAX)
{
    std::vector<ptrdiff_t> buf;
    return diff(aoff, n, boff, m, equal, edit, cancel, dmax, buf);
}

// ****************************************************************************
//
// diff -- implementation
//

template <class equal_t, class edit_t>
ptrdiff_t
diff_t<equal_t, edit_t>::find_middle_snake(
    size_t aoff, size_t n, size_t boff, size_t m, middle_snake& ms)
{
    ptrdiff_t delta = ptrdiff_t(n) - ptrdiff_t(m); // center diagonal of reverse search
    ptrdiff_t odd = delta & 1;                     // is it odd?
    ptrdiff_t mid = (n + m) / 2 + odd;
    ptrdiff_t fkmin = -ptrdiff_t(m);                   // minimum valid diagonal of forward search
    ptrdiff_t rkmin = -ptrdiff_t(m);                   // minimum valid diagonal of reverse search
    ptrdiff_t fkmax = n;                               // maximum valid diagonal of forward search
    ptrdiff_t rkmax = n;                               // maximum valid diagonal of reverse search
    ptrdiff_t fkminmax = std::max(delta - mid, fkmin); // upper bound on minimum valid diagonal of forward search
    ptrdiff_t fkmaxmin = std::min(delta + mid, fkmax); // lower bound on maximum valid diagonal of forward search
    ptrdiff_t rkminmax = std::max(-mid, rkmin);        // upper bound on minimum valid diagonal of reverse search
    ptrdiff_t rkmaxmin = std::min(+mid, rkmax);        // lower bound on maximum valid diagonal of reverse search
    ptrdiff_t fmin = 0;                                // actual limits of forward search
    ptrdiff_t fmax = 0;
    ptrdiff_t rmin = delta; // actual limits of reverse search
    ptrdiff_t rmax = delta;
    ptrdiff_t d;

    setfv(0, 0);
    setrv(delta, n);

    for (d = 1;; d++)
    {
        ptrdiff_t k, x, y;

        if ((2 * d - 1) >= dmax)
        {
            return dmax;
        }

        // test for termination
        if (cancel)
            throw diff_exception();

        // adjust the limits and define out of bound values for forward search
        if (fmin > fkmin)
            setfv(--fmin - 1, -1);
        else
        {
            if (fkmin <= fkminmax)
                ++fmin, ++fkmin;
            else
                --fmin, --fkmin;
        }
        if (fmax < fkmax)
            setfv(++fmax + 1, -1);
        else
        {
            if (fkmax >= fkmaxmin)
                --fmax, --fkmax;
            else
                ++fmax, ++fkmax;
        }
        for (k = fmax; k >= fmin; k -= 2)
        {
            ptrdiff_t xlo = fv(k - 1), xhi = fv(k + 1);
            if (xlo < xhi)
            {
                x = xhi;
            }
            else
            {
                x = xlo + 1;
            }
            y = x - k;
            ms.x = x; // remember begining of the snake

            while (x < ptrdiff_t(n) && y < ptrdiff_t(m) && equal(aoff + x, boff + y))
            {
                x++;
                y++;
            }
            setfv(k, x);

            if (odd && k >= rmin && k <= rmax)
            {
                if (x >= rv(k))
                {
                    ms.y = ms.x - k;
                    ms.u = x;
                    ms.v = y;
                    return 2 * d - 1;
                }
            }
        }

        // adjust the limits and define out of bound values for reverse search
        if (rmin > rkmin)
            setrv(--rmin - 1, std::numeric_limits<ptrdiff_t>::max());
        else
        {
            if (rkmin <= rkminmax)
                ++rmin, ++rkmin;
            else
                --rmin, --rkmin;
        }
        if (rmax < rkmax)
            setrv(++rmax + 1, std::numeric_limits<ptrdiff_t>::max());
        else
        {
            if (rkmax >= rkmaxmin)
                --rmax, --rkmax;
            else
                ++rmax, ++rkmax;
        }
        for (k = rmax; k >= rmin; k -= 2)
        {
            ptrdiff_t xlo = rv(k - 1), xhi = rv(k + 1);
            if (xlo < xhi)
            {
                x = xlo;
            }
            else
            {
                x = xhi - 1;
            }
            y = x - k;
            ms.u = x; // remember end of the snake

            while (x > 0 && y > 0 && equal(aoff + (x - 1), boff + (y - 1)))
            {
                x--;
                y--;
            }
            setrv(k, x);

            if (!odd && k >= fmin && k <= fmax)
            {
                if (x <= fv(k))
                {
                    ms.v = ms.u - k;
                    ms.x = x;
                    ms.y = y;
                    return 2 * d;
                }
            }
        }
    }
}

template <class equal_t, class edit_t>
ptrdiff_t
diff_t<equal_t, edit_t>::ses(size_t aoff, size_t n, size_t boff, size_t m)
{
    middle_snake ms;
    ptrdiff_t d;

    if (n == 0)
    {
        edit(ed_insert, boff, m);
        d = m;
    }
    else if (m == 0)
    {
        edit(ed_delete, aoff, n);
        d = n;
    }
    else
    {
        // Find the middle "snake" around which we recursively solve the
        // sub-problems.
        d = find_middle_snake(aoff, n, boff, m, ms);
        if (d >= dmax)
        {
            return dmax;
        }

        ses(aoff, ms.x, boff, ms.y);

        edit(ed_match, aoff + ms.x, ms.u - ms.x);

        aoff += ms.u;
        boff += ms.v;
        n -= ms.u;
        m -= ms.v;
        ses(aoff, n, boff, m);

        /*
    } else {
      ptrdiff_t x = ms.x;
      ptrdiff_t u = ms.u;

      // There are only 4 base cases when the
      // edit distance is 1.
      // 
      // n > m   m > n
      // 
      //   -       |
      //    \       \    x != u
      //     \       \
      // 
      //   \       \
      //    \       \    x == u
      //     -       |

      if (m > n) {
	if (x == u) {
	  edit(ed_match, aoff, n);
	  edit(ed_insert, boff + (m - 1), 1);
	} else {
	  edit(ed_insert, boff, 1);
	  edit(ed_match, aoff, n);
	}
      } else {
	if (x == u) {
	  edit(ed_match, aoff, m);
	  edit(ed_delete, aoff + (n - 1), 1);
	} else {
	  edit(ed_delete, aoff, 1);
	  edit(ed_match, aoff + 1, m);
	}
      }
    }
    */
    }

    return d;
}

template <class equal_t, class edit_t>
ptrdiff_t
diff_t<equal_t, edit_t>::operator()(
    size_t aoff, size_t n, size_t boff, size_t m)
{
    ptrdiff_t d;
    size_t p, s;

    // The ses function assumes the SES will begin and end with a delete or
    // insert. The following will insure this is true by eating any beginning
    // matches. This is also a quick to process sequences that match entirely.

    // eat common prefix
    p = 0;
    while (p < n && p < m && equal(aoff + p, boff + p))
    {
        p++;
    }
    // report common prefix
    edit(ed_match, aoff, p);

    // eat common suffix
    s = 0;
    while (s < n - p && s < m - p && equal(aoff + n - s - 1, boff + m - s - 1))
    {
        s++;
    }

    // reserve memory for search buffers
    size_t vsize = n - p - s + m - p - s + 3;
    if (vsize > 3)
    {
        buf.resize(2 * vsize);
        fvi = buf.begin() + (m - p - s + 1);
        rvi = buf.begin() + (vsize + m - p - s + 1);

        // do the comparison
        d = ses(aoff + p, n - p - s, boff + p, m - p - s);
    }
    else
    {
        d = 0;
    }

    // report common suffix
    edit(ed_match, aoff + n - s, s);

    return d;
}
