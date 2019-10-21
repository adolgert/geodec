#include <set>
#include <list>
#include <functional>
#include <boost/unordered_map.hpp>
#include <boost/pending/disjoint_sets.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include "CGAL/centroid.h"


namespace geodec
{


    /*! I have an iterator to facets but want an iterator to facet ids.
     *  FITER is a Facet_const_iterator.
     */
    template<class FITER>
    class facet_id_iterator
        : public boost::iterator_facade<
        facet_id_iterator<FITER>,
        const size_t,
        boost::forward_traversal_tag
        >
    {
        FITER m_;
    public:
        facet_id_iterator() : m_(0) {}
        facet_id_iterator(FITER m) : m_(m) {}
    private:
        friend class boost::iterator_core_access;
        void increment() { m_++; }
        bool equal(facet_id_iterator const& other) const
        {
            return this->m_ == other.m_;
        }
        const size_t& dereference() const { return m_->id(); }
    };


	template<class PAIR>
	struct pair_get_first : std::unary_function<const PAIR&,typename PAIR::first_type>
	{
		typename PAIR::first_type operator()(const PAIR& pair) const
		{
			return pair.first;
		}
	};
	
	
	
    /*! This functor runs adds faces of a complex to a disjoint_set.
     * Region is a CGAL Polyhedron.
     * Compare is the type of a functor that compares two faces.
     */
    template<class Region,class Compare>
	class disjoint_set_cluster
	{
    public:
        typedef typename Region::Facet_const_handle Facet_const_handle;
        typedef typename Region::Facet_const_iterator Facet_const_iterator;
        typedef typename Region::Halfedge_around_facet_const_circulator
                                                HF_const_circulator;

        //! Maps from element to count of elements in set.
        typedef boost::unordered_map<size_t,size_t> rank_t;
        //! Maps from element to parent of element.
        typedef boost::unordered_map<size_t,size_t> parent_t;
        typedef boost::associative_property_map<rank_t> rank_pmap_t;
        typedef boost::associative_property_map<parent_t> parent_pmap_t;
      
        typedef boost::disjoint_sets<rank_pmap_t,parent_pmap_t> dset_t;

        rank_t        rank_map_;
        parent_t      parent_map_;
        rank_pmap_t   rank_pmap_;
        parent_pmap_t parent_pmap_;
        dset_t        dset_;

        Compare compare_;
    public:

        disjoint_set_cluster(Compare compare) : compare_(compare),
                rank_pmap_(rank_map_), parent_pmap_(parent_map_),
                dset_(rank_pmap_,parent_pmap_)
        {
        }


        void operator()(const Region& region) {
            size_t facet_cnt=0;
            Facet_const_iterator last_facet = region.facets_end();
            Facet_const_iterator f = region.facets_begin();
            do {
                if (parent_map_.find(f->id())==parent_map_.end()) {
                    dset_.make_set(f->id());
                }
                HF_const_circulator h = f->facet_begin();
                do {
                    typename Region::Halfedge_const_handle opp = h->opposite();
                    if ( !opp->is_border() ) {
                        Facet_const_handle g = opp->facet();
                        if (parent_map_.find(g->id())==parent_map_.end()) {
                            dset_.make_set(g->id());
                        }
                        if (compare_(f->id(),g->id())) {
                            dset_.union_set(f->id(),g->id());
                        }
                    } else {
                        ; //std::cout << "facet is null" << std::endl;
                    }
                } while ( ++h != f->facet_begin() );
                facet_cnt++;
            } while ( ++f != last_facet );

            // Compress so that the parent is the ultimate parent.
            auto id_begin = facet_id_iterator<Facet_const_iterator>(
                                                region.facets_begin());
            auto id_end = facet_id_iterator<Facet_const_iterator>(
                                                region.facets_end());
            dset_.compress_sets(id_begin, id_end);

			// Walk clusters in order to find representatives and neighbors.
            std::list<Facet_const_iterator> unexplored;
            unexplored.push_back(region.facets_begin());
			std::map<size_t,size_t> edges;

			typedef typename Region::Point_3 Point_3;
			typedef typename Region::Traits::Kernel::Vector_3 Vector_3;
			
            while (!unexplored.empty()) {
                auto f=unexplored.front();
                unexplored.pop_front();
                bool border_cluster=false;

				size_t id = parent_map_[f->id()];

                std::list<Facet_const_iterator> cluster_next;
                cluster_next.push_back(f);
                std::set<Facet_const_iterator> cluster_seen;
				typedef std::map<Point_3,size_t> centroid_type;
				centroid_type centroids;
                while (!cluster_next.empty()) {
                    auto next=cluster_next.front();
                    cluster_next.pop_front();
                    cluster_seen.insert(next);

					Vector_3 midpoint;
					size_t vertex_cnt=0;
                    HF_const_circulator h = next->facet_begin();
                    do {
						vertex_cnt++;
						midpoint=midpoint+Vector_3(CGAL::ORIGIN, h->vertex()->point());
	
                        typename Region::Halfedge_const_handle opp = h->opposite();
                        if ( !opp->is_border() ) {
                            Facet_const_handle g = opp->facet();
							bool b_seen=(cluster_seen.find(g)!=cluster_seen.end());
							if (!b_seen) {
								size_t gparent=parent_map_[g->id()];
                            	if (gparent == id) {
	                                cluster_next.push_back(g);
	                            } else {
									edges.insert(std::pair<size_t,size_t>(id, gparent));
                            	}
							}
                        } else {
                            border_cluster=true;
                        }
                    } while ( ++h != next->facet_begin() );
					midpoint = midpoint / vertex_cnt;
					centroids.insert(std::pair<Point_3,size_t>(CGAL::ORIGIN+midpoint, f->id()));
                } // !cluster_next.empty()

				typedef pair_get_first<typename centroid_type::value_type> centroid_get_pt;
				typedef boost::transform_iterator<centroid_get_pt,typename centroid_type::const_iterator>
					cent_pt_type;
				cent_pt_type cent_begin(centroids.begin(), centroid_get_pt());
				cent_pt_type cent_end(centroids.end(), centroid_get_pt());
				Point_3 cluster_centroid = CGAL::centroid(cent_begin, cent_end);
				
				// Find id of cluster that is closest to centroid.
				auto close_begin=centroids.begin();
				auto close_end=centroids.end();
				auto closest=close_begin;
				while (close_begin!=close_end) {
					if (CGAL::has_smaller_distance_to_point(cluster_centroid,close_begin->first,closest->first))
					{
						closest=close_begin;
					}
					close_begin++;
				}
            } // !unexplored.empty()
        }
    };








    /*! This functor runs adds faces of a complex to a disjoint_set.
     * Region is a CGAL Polyhedron.
     * Compare is the type of a functor that compares two faces.
     */
    template<class Region,class Compare>
	class neighbor_face_cluster
	{
    public:
        typedef typename Region::Facet_const_handle Facet_const_handle;
        typedef typename Region::Facet_const_iterator Facet_const_iterator;
        typedef typename Region::Halfedge_around_facet_const_circulator
                                                HF_const_circulator;

        //! Maps from element to count of elements in set.
        typedef boost::unordered_map<size_t,size_t> rank_t;
        //! Maps from element to parent of element.
        typedef boost::unordered_map<size_t,size_t> parent_t;
        typedef boost::associative_property_map<rank_t> rank_pmap_t;
        typedef boost::associative_property_map<parent_t> parent_pmap_t;
      
        typedef boost::disjoint_sets<rank_pmap_t,parent_pmap_t> dset_t;

        rank_t        rank_map_;
        parent_t      parent_map_;
        rank_pmap_t   rank_pmap_;
        parent_pmap_t parent_pmap_;
        dset_t        dset_;

        Compare compare_;
    public:

        neighbor_face_cluster(Compare compare) : compare_(compare),
                rank_pmap_(rank_map_), parent_pmap_(parent_map_),
                dset_(rank_pmap_,parent_pmap_)
        {
        }


        void operator()(const Region& region) {
            std::list<Facet_const_iterator> unexplored;
            unexplored.push_back(region.facets_begin());
            size_t facet_cnt=0;

            while (!unexplored.empty()) {
                auto f=unexplored.front();
                unexplored.pop_front();
                bool border_cluster=false;

                std::list<Facet_const_iterator> cluster_next;
                cluster_next.push_back(f);
                std::set<Facet_const_iterator> cluster_seen;
                while (!cluster_next.empty()) {
                    auto next=cluster_next.front();
                    cluster_next.pop_front();
                    cluster_seen.insert(next);

                    HF_const_circulator h = next->facet_begin();
                    do {
                        typename Region::Halfedge_const_handle opp = h->opposite();
                        if ( !opp->is_border() ) {
                            Facet_const_handle g = opp->facet();
                            if (cluster_seen.find(g)!=cluster_seen.end()) {
                                cluster_next.push_back(g);
                            } else {
                                ;
                            }
                        } else {
                            border_cluster=true;
                        }
                    } while ( ++h != next->facet_begin() );
                    facet_cnt++;
                } // !cluster_next.empty()
            } // !unexplored.empty()
        }
    };




    template<class Region,class Compare>
	class neighbor_boundary_cluster
	{
    public:
        typedef typename Region::Facet_const_handle Facet_const_handle;
        typedef typename Region::Facet_const_iterator Facet_const_iterator;
        typedef typename Region::Halfedge_around_facet_const_circulator
                                                HF_const_circulator;

        //! Maps from element to count of elements in set.
        typedef boost::unordered_map<size_t,size_t> rank_t;
        //! Maps from element to parent of element.
        typedef boost::unordered_map<size_t,size_t> parent_t;
        typedef boost::associative_property_map<rank_t> rank_pmap_t;
        typedef boost::associative_property_map<parent_t> parent_pmap_t;
      
        typedef boost::disjoint_sets<rank_pmap_t,parent_pmap_t> dset_t;

        rank_t        rank_map_;
        parent_t      parent_map_;
        rank_pmap_t   rank_pmap_;
        parent_pmap_t parent_pmap_;
        dset_t        dset_;

        Compare compare_;
    public:

        neighbor_boundary_cluster(Compare compare) : compare_(compare),
                rank_pmap_(rank_map_), parent_pmap_(parent_map_),
                dset_(rank_pmap_,parent_pmap_)
        {
        }


        void operator()(const Region& region) {
            // Find a facet on a boundary of any kind.
            typedef typename Region::Halfedge_const_handle
                Halfedge_const_handle;
            auto half_bound_iter=region.halfedges_begin();
            while (half_bound_iter!=region.halfedges.end()) {
                bool edge=half_bound_iter->opposite()->is_border();
                if (edge) {
                    break;
                } else {
                    
                }                
                half_bound_iter++;
            }
        }
    };

} // end namespace
