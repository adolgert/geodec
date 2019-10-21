#ifndef _SIMPLEX_HPP_
#define _SIMPLEX_HPP_ 1

#include <map>
#include <set>
#include <boost/array.hpp>
#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/iterator/iterator_traits.hpp>
#include <boost/mpl/int.hpp>


namespace geodec {

/*! Count even or odd permutations of integers
 *  Given a list of integers, count how many permutations
 *  it would take to reorder them to this order and return
 *  whether it is even or odd.
 */
int permutation_parity(int *const seq, int cnt)
{
    int cycle_cnt=0;
    bool seen[cnt];
    for (int idx=0; idx<cnt; idx++) {
        if (seen[idx]==false) {
            cycle_cnt+=1;
            int j = idx;
            while (true) {
                seen[j]=true;
                j = seq[j];
                if (j == idx) break;
            }
        }
    }
    return (cnt - cycle_cnt) % 2;
}



/*! Given a string a and a string b, return the relative parity.
 *  T should be a forward iterator.
 */
template<class ITERA, class ITERB>
int relative_parity(ITERA a_begin, ITERB b_begin, int N)
{
    // Find the index of every member of b within a.
    typedef typename boost::iterator_value<ITERA>::type T;

    std::map<T, int> idx;
    
    for (int cnt=0; cnt<N; cnt++) {
        idx[*a_begin++]=cnt;
    }
    int permutation[N];
    for ( int *perm=permutation; perm!=permutation+N; b_begin++, perm++) {
        *perm=idx[*b_begin];
    }
    return permutation_parity( permutation, N );
}




/*! Iterate through all but the nth element */
template<class ITER>
class except_n_iterator
    : public boost::iterator_adaptor<
    except_n_iterator<ITER>,
    ITER,
    boost::use_default,
    boost::forward_traversal_tag
    >
{
    size_t _n;
    size_t _i;
public:
    except_n_iterator()
        : except_n_iterator::iterator_adaptor_(0)
    {}
    explicit except_n_iterator(ITER iter, size_t n)
        :  _i(0), _n(n), except_n_iterator::iterator_adaptor_(iter)
    {
        skip_if_n();
    }
private:
    friend class boost::iterator_core_access;
    void increment() {
        this->base_reference()++;
        _i++;
        skip_if_n();
    }
    void skip_if_n() {
        if (_i == _n) {
            increment();
        }
    }
};



template<class SIMPLEX>
class boundary_iterator_t
	: public boost::iterator_facade<
		boundary_iterator_t<SIMPLEX>,
		const SIMPLEX,
		boost::forward_traversal_tag
		>
{
public:
	typedef SIMPLEX simplex_type;
	typedef typename SIMPLEX::value_type T;
	typedef typename SIMPLEX::const_iterator subiter;
private:
	SIMPLEX _simplex;
	int _remove_idx;
	subiter _begin; // iterator over vertex ids.
	int _parity;
public:
    boundary_iterator_t() {}
    explicit boundary_iterator_t(subiter begin, int parity)
		: _begin(begin), _parity(parity), _remove_idx(0) {
		assign_simplex();
    }
	explicit boundary_iterator_t(int end_idx) : _remove_idx(end_idx) {}
private:
	friend class boost::iterator_core_access;
	
	void increment() {
		_remove_idx++;
		assign_simplex();
	}
	
	bool equal(boundary_iterator_t const& other) const {
		return other._remove_idx==_remove_idx;
	}
	
	const SIMPLEX& dereference() const { return _simplex; }
	
	void assign_simplex() {
        auto iter=except_n_iterator<subiter>(_begin, _remove_idx);
        _simplex.assign(iter, _parity);
	}
};



struct less_parity
{
    int &even;
    less_parity(int& parity) : even(parity) {}
    bool operator()(int a, int b) {
        bool res = (a<b);
        if (res) {
            even=!even;
        }
        return res;
    }
};




/*! This is a POD representing a simplex.
 *  Plain Old Datatype because it has no constructor.
 */
template<class T, int M>
class simplex : public boost::array<T,M+1>
{
public:
    int parity;
    typedef T value_type;
	static const int dimension=M;
    typedef simplex<T,M-1> boundary_type;
	typedef boundary_iterator_t<boundary_type> boundary_iterator;
public:
	/*! This assignment mirrors the one in pydec. It sorts the 
	 *  vertices, then checks parity.
	 */
    template<class InputIterator>
    void assign2(InputIterator begin, int b_parity=0) {
        std::copy_n(begin, this->size(), this->begin());
        std::sort(this->begin(), this->end());
        parity=relative_parity(begin, this->begin(), this->size())
                   ^ b_parity;
    }

	/*! This assigment tracks changes to parity during sorting
	 *  the vertices.
	 */
    template<class InputIterator>
    void assign(InputIterator begin, int b_parity=0) {
        std::copy_n(begin, this->size(), this->begin());
		int reorder_parity=0;
		std::sort(this->begin(), this->end(), less_parity(reorder_parity));
        parity=reorder_parity ^ b_parity;
    }


	std::pair<boundary_iterator,boundary_iterator>
	boundary() const {
		auto start=boundary_iterator(this->begin(), parity);
		auto finish=boundary_iterator(M+1);
		return std::pair<boundary_iterator,boundary_iterator>(start,finish);
	}
};


template<class T, int M>
std::ostream& operator<<(std::ostream& os, const simplex<T,M>& s)
{
	os << '(';
	for (size_t i=0; i<s.size()-1; i++) {
		os << s[i] << ' ';
	}
	os << s[s.size()-1] << ')';
	if (s.parity) {
		os << '-'; // odd
	} else {
		os << '+'; // even
	}
	return os;
}


template<class STORAGE>
class simplicial_complex {
	STORAGE& _storage;
public:
	typedef typename STORAGE::value_type simplex_type;
	typedef typename simplex_type::boundary_type boundary_type;

	simplicial_complex(STORAGE& storage) : _storage(storage) {}

	int manifold_dimension() {
		return simplex_type::dimension;
	}

    std::set<boundary_type> boundary() {
        std::set<boundary_type> bdry;
        auto begin=_storage.storage_begin();
        auto end=_storage.storage_end();
		for ( ; begin!=end; begin++) {
			typename simplex_type::boundary_iterator bd_begin, bd_end;
			for (std::tie(bd_begin,bd_end) = begin->boundary();
				 bd_begin!=bd_end;
				 bd_begin++) {
				auto elem = bdry.find(*bd_begin);
				if (elem==bdry.end()) {
					bdry.insert(*bd_begin);
				} else {
					bdry.erase(elem);
				}
			}
		}
		return bdry;
    }

};



// same for element_storage.

template<class VERTICES, class ELEMENTS>
class simplicial_mesh {
    VERTICES& _vertices;
    ELEMENTS& _elements;

public:
    typedef typename ELEMENTS::value_type simplex_type;
    typedef typename simplex_type::boundary_type boundary_type;

    simplicial_mesh(VERTICES& vertices, ELEMENTS& elements)
        : _vertices(vertices), _elements(elements) {}

    int manifold_dimension()
    { // Should be the dimension of the largest element.
        return get(_elements, 0).dimension;
    }

    int embedding_dimension()
    {
        return get(_vertices, 0).size();
    }

};

}


#endif // _SIMPLEX_HPP_
