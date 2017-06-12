#ifndef sho__h__
#define sho__h__

#include <cstdint>
#include <cassert>
#include <stdexcept>
#include <algorithm>
#include <utility>
#include <iterator>

// ---------------------------------------------------------------------
//            S M A L L    H A S H     O P T I M I Z A T I O N
//            ------------------------------------------------
// 
// When you have a lot of very small hash maps with just zero to a couple 
// entries => store a few element in a small array of possible, else 
//            allocate a full hash_map.
//
// Assumptions:
//     - we assume that the value of a pointer is always greater then N.
// ---------------------------------------------------------------------

// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
template <size_t N,
          template <class, class, class, class, class> class RefMap,
          class K,
          class V, 
          class Hash  = std::hash<K>, 
          class Pred  = std::equal_to<K>, 
          class Alloc = std::allocator<std::pair<const K, V> > >
class sho
{
public:
    typedef RefMap<K, V, Hash, Pred, Alloc>     Map;
    typedef typename Map::key_type              key_type;
    typedef typename Map::mapped_type           mapped_type;
    typedef typename Map::value_type            value_type;
    typedef typename Map::hasher                hasher;
    typedef typename Map::key_equal             key_equal;
    typedef typename Map::allocator_type        allocator_type;
    typedef std::pair<K, V>                     mutable_value_type;

    typedef typename Map::size_type             size_type;
    typedef typename Map::difference_type       difference_type;

    typedef typename Map::iterator              map_iterator;
    typedef typename Map::const_iterator        map_const_iterator;

    // ----------------
    template <class Val, class MapIt>
    class Iterator : public std::iterator<std::bidirectional_iterator_tag, Val>
    {
    public:
        typedef Val value_type;

        template <class CV, class CIt>
        Iterator(const Iterator<CV, CIt> &o) : _p((value_type *)o._p), _it(*((MapIt *)&o._it)) {}

        explicit Iterator(const value_type *p = 0) : _p((value_type *)p) {}
        explicit Iterator(const MapIt &o) : _p(0), _it(o) {}

        Iterator& operator++()   { if (_p) ++_p; else ++_it; return *this; }
        Iterator& operator--()   { if (_p) --_p; else --_it; return *this; }
        Iterator operator++(int) { Iterator tmp(*this); ++*this; return tmp; }
        Iterator operator--(int) { Iterator tmp(*this); --*this; return tmp; }

        bool operator==(const Iterator &o) const { return _p ? _p == o._p : _it == o._it; }
        bool operator!=(const Iterator &o) const { return !(*this == o); }

        value_type& operator*() const  { return _p ? *_p : *_it; }
        value_type* operator->() const { return &(operator*()); }
        
        const MapIt& getMapIt() const { assert(!_p); return _it; }
        value_type * getPtr() const   { assert(_p); return _p; }

    private:
        friend class sho;
        
        value_type *_p;
        MapIt       _it;
    };

    // ----------------
    typedef Iterator<value_type, map_iterator>              iterator;
    typedef Iterator<const value_type, map_const_iterator>  const_iterator;

    sho(size_type = 0) : _cnt(0) {}

    ~sho() 
    { 
        if (_hasMap()) 
            _cleanMap();
    }
        
    sho(const sho &o) : _cnt(0) 
    {
        if (o._hasMap())
            _cnt = (uintptr_t)(new Map(o._getMap()));
        else
            _copyLocal(o);
    }

    sho& operator=(const sho &o) 
    {
        if (this == &o)
            return *this;

        if (_hasMap()) 
            _cleanMap();
        if (o._hasMap()) 
            _cnt = (uintptr_t)(new Map(o._getMap()));
        else
            _copyLocal(o);
        return *this;
    }
    
    // Iterator functions
    iterator begin() const             
    {
        return _hasMap() ? iterator(_getMap()->begin()) : iterator(&_items[0]); 
    }
    
    iterator end() const             
    {
        return _hasMap() ? iterator(_getMap()->end()) : iterator(&_items[_cnt]); 
    }

    const_iterator cbegin() const            
    {
        return _hasMap() ? const_iterator(_getMap()->begin()) : const_iterator(&_items[0]); 
    }
    
    const_iterator cend() const             
    {
        return _hasMap() ? const_iterator(_getMap()->end()) : const_iterator(&_items[_cnt]); 
    }
 
    iterator find(const key_type& key)
    {
        if (_hasMap())
            return iterator(_getMap()->find(key));
        else
            for (size_t i=0; i<_cnt; ++i)
                if (key_equal()(_items[i].first, key))
                    return iterator(&_items[i]); 
        return end();
    }

    const_iterator find(const key_type& key) const
    {
        if (_hasMap())
            return const_iterator(_getMap()->find(key));
        else
            for (size_t i=0; i<_cnt; ++i)
                if (key_equal()(_items[i].first, key))
                    return const_iterator(&_items[i]); 
        return cend();
    }

    size_type erase(const key_type& k)
    {
        if (_hasMap())
            return _getMap()->erase(k);

        for (size_t i=0; i<_cnt; ++i)
        {
            if (key_equal()(_items[i].first, k))
            {
                // rotate item to delete to last position
                std::rotate((mutable_value_type *)&_items[i], 
                            (mutable_value_type *)&_items[i+1], 
                            (mutable_value_type *)&_items[_cnt]);
                --_cnt;
                allocator_type().destroy(&_items[_cnt]);
                return 1;
            }
        }
        return 0;
    }

    iterator erase(const_iterator it)
    {
        if (_hasMap())
        {
            map_iterator res = _getMap()->erase(it.getMapIt());
            return iterator(res);
        }
        const value_type *cur = it.getPtr();
        size_t idx = cur - &_items[0];
        assert(idx <= N);

        if (idx < N && _cnt)
        {
            // rotate item to delete to last position
            std::rotate((mutable_value_type *)cur, 
                        (mutable_value_type *)cur+1, 
                        (mutable_value_type *)&_items[_cnt]);
            --_cnt;
            allocator_type().destroy(&_items[_cnt]);
            return iterator(cur);
        }
        return end();
    }
        
    mapped_type& operator[](const key_type& key)
    {
        if (_hasMap())
            return _getMap()->operator[](key);
        else
        {
            for (size_t i=0; i<_cnt; ++i)
                if (key_equal()(_items[i].first, key))
                    return _items[i].second;
        }
        insert(value_type(key, mapped_type()));
        return this->operator[](key);
    }

    mapped_type& at(const key_type& key)
    {
        if (_hasMap())
            return _getMap()->at(key); // throws if not found

        for (size_t i=0; i<_cnt; ++i)
            if (key_equal()(_items[i].first, key))
                return _items[i].second;
        throw std::out_of_range("at: key not present");
    }


    std::pair<iterator, bool> insert(const value_type& val)
    {
        if (_hasMap())
        {
            std::pair<map_iterator, bool> res = _getMap()->insert(val);
            return std::make_pair(iterator(res.first), res.second);
        }

        for (size_t i=0; i<_cnt; ++i)
            if (key_equal()(_items[i].first, val.first))
                return std::make_pair(iterator(&_items[i]), false);

        // not present ... insert it
        if (_cnt == N)
        {
            _switchToMap();
            std::pair<map_iterator, bool> res = _getMap()->insert(val);
            assert(res.second);
            return std::make_pair(iterator(res.first), true);
        }
        *((K *)(&_items[_cnt].first)) = val.first;
        _items[_cnt].second = val.second;
        return std::make_pair(iterator(&_items[_cnt++]), true);
    }

    size_type size() const { return _hasMap() ? _getMap()->size() : _cnt; }

    size_type bucket_count() const { return _hasMap() ? _getMap()->bucket_count() : _cnt; }

    size_type count (const key_type& k) const
    {
        return find(k) == cend() ? 0 : 1;
    }

    bool empty() const { return size() == 0; }

    void clear() 
    {
        if (_hasMap())
            _cleanMap();
        else
        {
            allocator_type alloc;
            for (size_t i=0; i<_cnt; ++i)
                alloc.destroy(&_items[i]);
            _cnt = 0;
        }
    }
        
private:
    bool   _hasMap() const { return _cnt > N; }
    
    Map *  _getMap() const { assert(_hasMap()); return (Map *)_cnt; }

    void   _cleanMap() { delete _getMap(); _cnt = 0; }

    void   _copyLocal(const sho &o) 
    {
        assert(!_hasMap());
        _cnt = o._cnt;
        for (size_t i=0; i<_cnt; ++i)
            _items[i] = o._items[i];
    }

    void   _createMap()
    { 
        assert(!_hasMap()); 
        _cnt = (uintptr_t)(new Map);
    }

    void   _switchToMap()
    {
        assert(!_hasMap());

        uintptr_t cnt = _cnt;
        _cnt = 0;
        _createMap();

        Map *map = _getMap();
        for (size_t i=0; i<cnt; ++i)
            map->insert(_items[i]);
    }
 
    uintptr_t  _cnt;
    value_type _items[N];
};


#endif // sho__h__
