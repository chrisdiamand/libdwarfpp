/* dwarfpp: C++ binding for a useful subset of libdwarf, plus extra goodies.
 * 
 * encap.hpp: transparent, mutable representations 
 *			of libdwarf-like structures.
 *
 * Copyright (c) 2008--11, Stephen Kell.
 */

#ifndef DWARFPP_ENCAP_HPP_
#define DWARFPP_ENCAP_HPP_

#include "spec.hpp"
#include "lib.hpp"
#include "attr.hpp"
#include "spec_adt.hpp"
#include <boost/optional.hpp>
#include <memory>
#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/iterator/filter_iterator.hpp>
#include <srk31/iterator_with_lens.hpp>
#include <srk31/downcasting_iterator.hpp>
#include <srk31/selective_iterator.hpp>
#include <srk31/concatenating_iterator.hpp>

#include <vector>
#include <set>
#include <map>
#include <string>
#include <utility>

// #include <execinfo.h> // for backtrace(), for debugging -- NOTE: not in BSD

namespace dwarf {
	namespace encap {
		using dwarf::spec::opt; // FIXME: should put opt in a different namespace?
		using namespace dwarf::lib;
		using std::dynamic_pointer_cast;
		using std::shared_ptr;
		using std::make_pair;
		using std::set;
		
		using srk31::concatenating_sequence;
		using srk31::concatenating_iterator;
		
		// forward declarations
		class die;
		class basic_die;
		class factory;
		class file_toplevel_die;
		template <typename Iter> 
		struct has_name
		{
			bool operator()(const Iter i) const
			{ return (*i)->has_attr(DW_AT_name); }
			bool operator==(const has_name& arg) const { return true; }
		};

		// convenience typedefs
		typedef std::vector<std::string> pathname;
		//typedef std::vector<Dwarf_Off> die_off_list;
		typedef std::vector<die*> die_ptr_list;

		// basic definitions for dealing with encap data
		class dieset 
		 : private std::map<Dwarf_Off, std::shared_ptr<dwarf::encap::die> >,
		   public virtual spec::abstract_mutable_dieset
		{
			typedef std::map<Dwarf_Off, std::shared_ptr<dwarf::encap::die> > super;
			friend class file;
			friend class die;
			bool destructing;
			/* We don't guarantee that DIEs' offsets are monotonic across any depthfirst
			 * or siblingwise traversal of the tree. This is so that we can always insert 
			 * new DIEs, even if there are no free offsets left in the range available
			 * under a given parent. However, for optimisations, it's useful to know that
			 * a particular range of DIEs is covered, so we remember where the nonmonotonic
			 * DIEs begin at. */
			Dwarf_Off last_monotonic_offset;
		public:
			bool is_destructing() const { return destructing; }
			Dwarf_Off get_last_monotonic_offset() const { return last_monotonic_offset; }
		private:
			friend struct Print_Action;
			const ::dwarf::spec::abstract_def *p_spec;
			void create_toplevel_entry();
			dieset() : destructing(false), last_monotonic_offset(0UL), p_spec(&spec::DEFAULT_DWARF_SPEC)
			{
				create_toplevel_entry();
				//std::cerr << "Default-constructed a dieset!" << std::endl;
			}
		public:
			typedef super::iterator map_iterator;
			typedef super::const_iterator map_const_iterator;
			typedef abstract_dieset::iterator abstract_iterator;
			typedef std::pair<Dwarf_Off, Dwarf_Half> backref_rec;
			typedef std::vector<backref_rec> backref_list;
			explicit dieset(const ::dwarf::spec::abstract_def& spec) 
			: destructing(false), last_monotonic_offset(0UL), p_spec(&spec) 
			{
				create_toplevel_entry();
				//std::cerr << "Non-default-constructed a dieset!" << std::endl;
			}
			virtual ~dieset() { destructing = true; }
			const ::dwarf::spec::abstract_def& spec() const { assert(p_spec); return *p_spec; }
			const ::dwarf::spec::abstract_def& get_spec() const { assert(p_spec); return *p_spec; }			
			std::shared_ptr<file_toplevel_die> all_compile_units();
			/* std::shared_ptr<file_toplevel_die> toplevel() { return all_compile_units(); } */
			struct pair_compare_by_key
			{
				bool operator()(const value_type& v1, const value_type& v2) const
				{ 
//				std::cerr << "comparing offsets " << v1.first << " and " << v2.first << std::endl;
					return v1.first < v2.first; 
				}
			};
			Dwarf_Off next_free_offset() const 
			{ 
				//std::cerr << "getting next free offset from dieset of size " << size() << std::endl;
				return std::max_element(
					this->super::begin(), this->super::end(), pair_compare_by_key())->first + 1; 
			}
			encap::dieset& operator=(const encap::dieset& arg);
			std::pair<map_iterator, bool> insert(const value_type& val)
			{
			   	//std::cerr << "inserted!" << std::endl;
			 	//if (val.first > est_lowest_free_offset) est_lowest_free_offset = val.first + 1;
				return this->super::insert(val);
			}
			map_iterator insert(map_iterator pos, const value_type& val)
			{
			   	//std::cerr << "inserted!" << std::endl;
				//if (val.first > est_lowest_free_offset) est_lowest_free_offset = val.first + 1;
				return this->super::insert(pos, val);
			}
			virtual 
			std::shared_ptr<dwarf::spec::basic_die> 
			insert(
				dwarf::lib::Dwarf_Off pos, 
				std::shared_ptr<dwarf::spec::basic_die> p_d)
			{
				/* We assume that the parent of the DIE is correctly set up. */
				
				/* We may only insert encap::die pointers into this dieset. */
				
				/* Note that DIEs are *immutable* -- if we want to create a 
				 * new one, we have to build it from scratch or by cloning
				 * an existing one. The built DIE only has to implement the
				 * non-mutable */
				std::shared_ptr<dwarf::encap::die> encap_d
				 = std::dynamic_pointer_cast<encap::die>(p_d);
				if (!encap_d) return std::shared_ptr<dwarf::spec::basic_die>(); // return null
				else 
				{
					if (super::find(pos) != super::end()) throw Error(0, 0); // FIXME: better error
					auto ret = super::insert(std::make_pair(pos, std::dynamic_pointer_cast<encap::die>(p_d)));
					assert(ret.second);
					return p_d;
				}
			}
			
			template <class In> void insert (In first, In last) 
			{
			   //	std::cerr << "inserted!" << std::endl;
				while(first != last) insert(*first++); // will use a local insert ^^^
			}
			
		private:
			std::map<Dwarf_Off, backref_list> m_backrefs;
		public:
			std::map<Dwarf_Off, backref_list>& backrefs() { return m_backrefs; }
//		private:
//			std::vector<encap::arangelist> m_aranges;
		public:
			//encap::arangelist arangelist_at(Dwarf_Unsigned i) const;
			
			map_iterator map_begin() { return this->super::begin(); }
			map_const_iterator map_begin() const { return this->super::begin(); }
			map_iterator map_end() { return this->super::end(); }
			map_const_iterator map_end() const { return this->super::end(); }
			map_const_iterator map_find(Dwarf_Off off) const { return this->super::find(off); }
			map_iterator map_find(Dwarf_Off off) { return this->super::find(off); }
			super::size_type map_size() const { return this->super::size(); }
			
			// override of abstract_dieset
			Dwarf_Off highest_offset_upper_bound() 
			{ return (--map_end())->first; }
			
//			Encap_all_compile_units& all_compile_units()
//			{ return dynamic_cast<Encap_all_compile_units&>(*(this->find(0UL)->second)); }
			
			// helper for find()
			// FIXME: delete this
			void build_path_from_root(abstract_dieset::path_type& path, map_iterator current);
			// "official" spec-defined API to the above
			//abstract_dieset::path_type path_from_root(Dwarf_Off);
			
			// override find() to be more efficient than the ADT version
			abstract_dieset::iterator find(dwarf::lib::Dwarf_Off off)
			{ auto found_iter = this->map_find(off);
			  if (found_iter != this->map_end())
			  { 
				path_type path;
				this->build_path_from_root(path, found_iter);
			  	return abstract_dieset::iterator(*this, off, path);
			  }
			  else return this->end(); }
			abstract_dieset::iterator begin()
			{ return abstract_dieset::iterator(*this, 0UL,
				path_type(1, 0UL)); }
			abstract_dieset::iterator end()
			{ return abstract_dieset::iterator(*this, 
				std::numeric_limits<Dwarf_Off>::max(),
				path_type()); }
			std::shared_ptr<dwarf::spec::basic_die> 
			operator[](dwarf::lib::Dwarf_Off off) const;
			std::shared_ptr<spec::file_toplevel_die> toplevel();
			//std::deque< spec::abstract_dieset::position > path_from_root(Dwarf_Off off);

			bool move_to_first_child(spec::abstract_dieset::iterator_base& arg);
			bool move_to_parent(spec::abstract_dieset::iterator_base& arg);
			bool move_to_next_sibling(spec::abstract_dieset::iterator_base& arg);
			inline Dwarf_Off find_parent_offset_of(Dwarf_Off off);
// 			// FIXME: delete this old cruft
// 			opt<die&> resolve_die_path(const Dwarf_Off start, 
// 				const pathname& path, pathname::const_iterator pos);
// 			opt<die&> resolve_die_path(const Dwarf_Off start, 
// 				const pathname& path) { return resolve_die_path(start, path, path.begin()); }
// 			opt<die&> resolve_die_path(const pathname& path) 
// 			{ return resolve_die_path(0UL, path); }
// 			opt<die&> resolve_die_path(const std::string& singleton_path) 
// 			{ return resolve_die_path(pathname(1, singleton_path)); }
			//friend std::ostream& operator<<(std::ostream& o, const dieset& ds);
			friend std::ostream& print_artificial(std::ostream& o, const dieset& ds);
		}; // end class dieset
		//std::ostream& operator<<(std::ostream& o, const dieset& ds);
		std::ostream& print_artificial(std::ostream& o, const dieset& ds);
		
		// forward decls
		class file;
		class die;		
		
		class file : public dwarf::lib::file
		{
			dieset m_ds;
			
			//die_off_list cu_off_list;
			std::map<Dwarf_Off, Dwarf_Half> cu_version_stamps;
			
			void encapsulate_die(lib::die& d, Dwarf_Off parent_off);
			//const dwarf::spec::abstract_def *p_spec;
			file() /*: p_spec(spec::DEFAULT_DWARF_SPEC)*/ {} // private constructor
		public:
			dieset& get_ds() { return m_ds; }
			const dieset& get_ds() const { return m_ds; }
			dieset& ds() { return m_ds; }
			file(int fd, Dwarf_Unsigned access = DW_DLC_READ);
			static file& default_file()
			{
				static file *pointer_to_default = 0;
				if (pointer_to_default == 0) pointer_to_default = new file();
				return *pointer_to_default;
			}
			//const dwarf::spec::abstract_def& get_spec() { return *p_spec; }
			/* DWARF info often omits imported function prototypes, so we hackily
			 * add these back in using libelf. */
			void add_imported_function_descriptions();
		}; // end class file
		

		template <typename Value> struct die_out_edge_iterator; // forward decl
		template <typename Value> struct sibling_dep_edge_iterator;

	} } namespace boost {
		std::pair<
			dwarf::encap::die_out_edge_iterator<dwarf::encap::attribute_value::weak_ref>, 
			dwarf::encap::die_out_edge_iterator<dwarf::encap::attribute_value::weak_ref> >
		out_edges(std::pair<dwarf::lib::Dwarf_Off, std::shared_ptr<dwarf::encap::die> >, const dwarf::encap::dieset&);	
	} namespace dwarf { namespace encap {

		// lenses for generating 
		struct die_ptr_offset_lens : public Lens<Dwarf_Off, die*>
		{
			dieset *p_dies;
			struct get_type : std::unary_function<Dwarf_Off, die*>
			{
				dieset *p_dies;
				die * operator()(Dwarf_Off off) const 
				{ 
					dieset::map_iterator i = p_dies->map_find(off);
					if (i == p_dies->map_end()) 
					{
						std::cerr << "ERROR: die_ptr_offset_lens applied to offset "
							<< off << " which doesn't map to an entry in the dieset."
							<< std::endl;
						assert(false);
					}
					return i->second.get(); 
				}
				get_type(dieset *p_dies) : p_dies(p_dies) {}
			};
			get_type get;
			die_ptr_offset_lens(dieset& dies) : p_dies(&dies), get(&dies) {}
			die_ptr_offset_lens() : p_dies(0), get(0) 
			{ 
				//std::cerr << "Warning: default-constructed a lens!" << std::endl; 
			}
			die_ptr_offset_lens& operator=(const die_ptr_offset_lens& arg)
			{
				//assert(&(arg.m_dies) == &m_dies);
				p_dies = arg.p_dies;
				get = arg.get;
				return *this;
			}
		};
		struct die_ptr_map_iter_lens : public Lens<dieset::map_iterator, die*>
		{
			struct get_type : std::unary_function<dieset::map_iterator, die*>
			{
				die * operator()(dieset::map_iterator iter) const 
				{ 
					//if (iter == m_dies.end()) 
					//{
					//	std::cerr << "ERROR: die_ptr_map_iter_lens applied to end iterator."
					//		<< std::endl;
					//	assert(false);
					//}
					return iter->second.get(); 
				}
			};
			get_type get;
		};

		class basic_die; // forward decl
		class dwarf3_factory_t; // HACK
		class die : public virtual spec::basic_die
		{
			friend struct die_out_edge_iterator<attribute_value::weak_ref>; // in encap_graph.hpp
			friend class factory;
			friend class dieset;
			friend class dwarf3_factory_t; // HACK
			friend std::pair<
				die_out_edge_iterator<attribute_value::weak_ref>, 
				die_out_edge_iterator<attribute_value::weak_ref> > 
			boost::out_edges(std::pair<dwarf::lib::Dwarf_Off, std::shared_ptr<dwarf::encap::die> >, const dwarf::encap::dieset&);
		protected:
			/* TODO: make this a handle/body implementation, to allow copying of DIEs
			 * without unnecessarily copying those vectors and maps around. */
			dieset& m_ds;
			shared_ptr<die> p_parent;
			Dwarf_Half m_tag;
			Dwarf_Off m_offset;
			Dwarf_Off cu_offset;
		public:
			std::map<Dwarf_Half, attribute_value> m_attrs;
		protected:
			std::set<Dwarf_Off> m_children;
			void attach_child(std::shared_ptr<encap::basic_die> p);
			
		public:
			typedef dwarf::encap::factory factory_type;
			typedef std::map<Dwarf_Half, attribute_value> attribute_map;
			
			
			struct is_ref_attr_t : public std::unary_function<attribute_map::value_type, bool>
			{
				bool operator()(attribute_map::value_type& arg) const
				{
					return arg.second.get_form() == attribute_value::REF
						&& arg.first != DW_AT_sibling; // HACK: don't count sibling attrs
				}
			} is_ref_attr;
			
			typedef boost::filter_iterator<is_ref_attr_t, attribute_map::iterator> ref_attrs_iterator;
			ref_attrs_iterator ref_attrs_begin() 
			{ return boost::make_filter_iterator(is_ref_attr, m_attrs.begin(), m_attrs.end()); }
			ref_attrs_iterator ref_attrs_end() 
			{ return boost::make_filter_iterator(is_ref_attr, m_attrs.end(), m_attrs.end()); }
			
			/* This concatenating iterator joins all ref attributes in DIEs found on a depthfirst walk. */
			typedef concatenating_sequence<ref_attrs_iterator> all_refs_dfs_sequence;
			typedef concatenating_iterator<ref_attrs_iterator> all_refs_dfs_iterator;
			/* Don't let clients _begin() and _end() separately, because
			 * this will construct two underlying sequence objects without
			 * any guarantee that they are comparable. The right thing to do
			 * is to get the sequence object, then use its begin() and end().  */
			std::shared_ptr<all_refs_dfs_sequence> all_refs_dfs_seq()
			{
				auto p_seq = std::make_shared<all_refs_dfs_sequence>();
				auto my_path = this->iterator_here().base().path_from_root;
				
				// To do a depth-first walk under this DIE, our termination condition is
				// that the prefix from the root is shared with the toplevel.
				
				// Build a sequence out of subsequences. Each subsequence is the
				// sequence of reference attributes that a given DIE has.
				// FIXME: make this "subtree depthfirst walk" a reusable traversal policy 
				for (auto i_dfs = this->iterator_here(); 
						i_dfs.base().path_from_root.size() >= my_path.size() &&
						my_path == spec::abstract_dieset::path_type(
								i_dfs.base().path_from_root.begin(), 
								i_dfs.base().path_from_root.begin() + my_path.size()
							);
						++i_dfs)
				{
					die& d = dynamic_cast<die&>(**i_dfs);
					//std::cerr << "Appending ref attrs from DIE at 0x" 
					//	<< std::hex << d.m_offset << std::dec << std::endl;
					p_seq->append(d.ref_attrs_begin(), d.ref_attrs_end());
				}
				return p_seq;				
			}			
			
			// fully specifying constructor
			die(dieset& ds, Dwarf_Off parent, Dwarf_Half tag, 
				Dwarf_Off offset, Dwarf_Off cu_offset, 
				const attribute_map& attrs, const set<Dwarf_Off>& children) :
				m_ds(ds), 
				p_parent((offset == 0UL) 
					? shared_ptr<encap::die>()
					: dynamic_pointer_cast<encap::die>(m_ds[parent])), 
				m_tag(tag), m_offset(offset), 
				cu_offset(cu_offset), m_attrs(attrs), m_children(children) 
				{ assert(offset == 0UL || p_parent); }
				
			die() : m_ds(file::default_file().get_ds()), 
				p_parent(), m_tag(0), m_offset(0), cu_offset(0),
				m_attrs(), m_children() 
			{
				std::cerr << "Warning: created dummy encap::die" << std::endl;
			} // dummy to support std::map<_, die> and []
		protected:
			die(dieset& ds, lib::die& d, Dwarf_Off parent_off); 
		public:
			die(dieset& ds, shared_ptr<lib::die> p_d, Dwarf_Off parent_off); 
		private:
			void initialize_from_lib_die(lib::die& d);
		public:
			die(const die& d); // copy constructor
			
			die& operator=(const die& d)
			{
				assert(&(this->m_ds) == &(d.m_ds)); // can only assign DIEs of same dieset
				
				// for now, can only assign sibling DIES
				assert(this->p_parent->get_offset() == d.p_parent->get_offset());
				
				// offset and cu_offset are *unchanged*!
				
				this->m_tag = d.m_tag;
				this->m_attrs = d.m_attrs;
				this->m_children = d.m_children;
				
				// FIXME: move child DIEs too -- i.e. set their parent off to us
				
				return *this;
			}
			
			virtual ~die();
			
			const dieset& get_ds() const { return m_ds; }
			encap::dieset& get_ds() { return m_ds; } // covariant return 
						
			Dwarf_Off get_offset() const { return m_offset; }
				
			attribute_map get_attrs() { return m_attrs; } // copying
			attribute_map& attrs() { return m_attrs; }
			const attribute_map& const_attrs() const { return m_attrs; }
			
			Dwarf_Half get_tag() const { return m_tag; }
			Dwarf_Half set_tag(Dwarf_Half v) { return m_tag = v; }
			
			Dwarf_Off parent_offset() const { return p_parent->get_offset(); }
			std::shared_ptr<spec::basic_die> get_parent() { return /*m_ds[m_parent];*/ p_parent; }
			Dwarf_Off get_first_child_offset() const
			{ if (m_children.size() > 0) return *m_children.begin();
			  else throw lib::No_entry(); }
			std::shared_ptr<spec::basic_die> get_first_child() 
			{ return m_ds[get_first_child_offset()]; }

			Dwarf_Off get_next_sibling_offset() const
			{ 	if (m_offset == 0UL) throw No_entry();
				auto& parent_children = p_parent->m_children;
				auto found = parent_children.find(m_offset);
				assert(found != parent_children.end());
				if (++found == parent_children.end()) throw lib::No_entry();
				assert(*found != m_offset);
				return *found; 
			}
			std::shared_ptr<spec::basic_die> get_next_sibling()
			{
				return m_ds[get_next_sibling_offset()]; 
			}
			
			set<Dwarf_Off>& children()  { return m_children; }
			const set<Dwarf_Off>& children() const { return m_children; }
			set<Dwarf_Off>& get_children() { return m_children; }
			const set<Dwarf_Off>& const_children() const { return m_children; }
			
			bool has_attr(Dwarf_Half at) const { return (m_attrs.find(at) != m_attrs.end()); }
			const attribute_value& get_attr(Dwarf_Half at) const { return (*this)[at]; }
			const attribute_value& operator[] (Dwarf_Half at) const 
			{ 
				if (has_attr(at)) return m_attrs.find(at)->second; 
				else throw No_entry();
			}
			attribute_value& put_attr(Dwarf_Half attr, attribute_value val); 
			//attribute_value& put_attr(Dwarf_Half attr, 
			//	Die_encap_base& target);
			attribute_value& put_attr(Dwarf_Half attr, 
				std::shared_ptr<basic_die> target);

			opt<std::string> 
			get_name() const { 
				if (has_attr(DW_AT_name)) return get_attr(DW_AT_name).get_string();
				else return 0; 
			}
			const spec::abstract_def& get_spec() const 
			{ return m_ds.get_spec(); }

			//friend std::ostream& operator<<(std::ostream& o, const dwarf::encap::die& d);
			void print(std::ostream& o) const
			{
				o << *this;
			}
		};
		
		inline Dwarf_Off dieset::find_parent_offset_of(Dwarf_Off off)
		{ auto found = map_find(off); assert(found != map_end());
		  return found->second->p_parent->get_offset(); } 

		
/****************************************************************/
/* begin generated ADT includes								 */
/****************************************************************/
#define forward_decl(t) struct t ## _die;
/* #define declare_base(base) base ## _die */
/* #define base_fragment(base) base ## _die(ds, p_d) {} */
/*#define initialize_base(fragment) virtual spec:: fragment ## _die(ds, p_d) */
#define constructor(fragment) \
			/* "full" constructor */ \
	fragment ## _die(dieset& ds, Dwarf_Off parent, \
				Dwarf_Off offset, Dwarf_Off cu_offset, \
				const encap::die::attribute_map& attrs, \
				const set<Dwarf_Off>& children) \
			 :	basic_die(ds, parent, DW_TAG_ ## fragment, offset, cu_offset, attrs, children) {} \
			/* "encap" constructor */ \
	protected: fragment ## _die(dieset& ds, lib::die& d, Dwarf_Off parent_off) \
			 :  basic_die(ds, d, parent_off) \
				{ Dwarf_Half t; d.tag(&t); assert(t == DW_TAG_ ## fragment); } \
	public: \
			/* "create" constructor */ \
	fragment ## _die(shared_ptr<encap::basic_die> parent, \
				opt<std::string> name = opt<string>()) \
			 :	basic_die(parent->get_ds(), parent->get_offset(), DW_TAG_ ## fragment, \
			 	parent->get_ds().next_free_offset(),  \
			 	0, encap::die::attribute_map(), set<Dwarf_Off>()) \
				{ if (name) put_attr(DW_AT_name, dwarf::encap::attribute_value( \
					parent->get_ds(), std::string(*name))); } 
				
#define begin_class(fragment, base_inits, base_decls...) \
	struct fragment ## _die : public encap::basic_die, public virtual spec:: fragment ## _die { \
		typedef fragment ## _die self_type; \
		friend class factory; \
		constructor(fragment)
#define base_initializations(...) __VA_ARGS__
#define end_class(fragment) \
	};

#define stored_type_string std::string
#define stored_type_flag bool
#define stored_type_unsigned Dwarf_Unsigned
#define stored_type_signed Dwarf_Signed
#define stored_type_offset Dwarf_Off
#define stored_type_half Dwarf_Half
#define stored_type_ref Dwarf_Off
#define stored_type_tag Dwarf_Half
#define stored_type_loclist dwarf::encap::loclist
#define stored_type_address dwarf::encap::attribute_value::address
#define stored_type_refdie std::shared_ptr<spec::basic_die> 
#define stored_type_refdie_is_type std::shared_ptr<spec::type_die> 
#define stored_type_rangelist dwarf::encap::rangelist

#define attr_optional(name, stored_t) \
	opt<stored_type_ ## stored_t> get_ ## name() const \
	{ if (has_attr(DW_AT_ ## name)) return (*this)[DW_AT_ ## name].get_ ## stored_t (); \
	  else return opt< stored_type_ ## stored_t>(); } \
	std::shared_ptr<self_type> set_ ## name(opt<stored_type_ ## stored_t> arg) { \
	if (arg) put_attr(DW_AT_ ## name, encap::attribute_value(this->m_ds, deref_opt(arg))); \
	else m_attrs.erase(DW_AT_ ## name); \
	return std::dynamic_pointer_cast<self_type>(this->shared_from_this()); }

#define super_attr_optional(name, stored_t) attr_optional(name, stored_t)

#define attr_mandatory(name, stored_t) \
	stored_type_ ## stored_t get_ ## name() const \
	{ assert (has_attr(DW_AT_ ## name)); return (*this)[DW_AT_ ## name].get_ ## stored_t (); } \
	/*std::shared_ptr<self_type>*/ void set_ ## name(stored_type_ ## stored_t arg) { \
	put_attr(DW_AT_ ## name, encap::attribute_value(this->m_ds, arg)); \
	/* return std::dynamic_pointer_cast<self_type>(this->shared_from_this());*/ }
// HACK: don't use shared_from_this until its interaction with multiple inheritance
// is fixed <http://lists.boost.org/Archives/boost/2010/11/173366.php>

#define super_attr_mandatory(name, stored_t) attr_mandatory(name, stored_t)
// NOTE: super_attr distinction is necessary because
// we do inherit from virtual DIEs in the abstract (spec) realm
// -- so we don't need to repeat definitions of attribute accessor functions there
// we *don't* inherit from virtual DIEs in the concrete realm
// -- so we *do* need to repeat definitions of these attribute accessor functions

#define child_tag(arg) 

#define extra_decls_compile_unit \
		Dwarf_Half get_address_size() const; \
		std::string source_file_name(unsigned o) const;	\
		unsigned source_file_count() const;
			
		class basic_die : public die, public virtual spec::basic_die
		{
		private:
			friend class factory;
			typedef basic_die self;
			typedef basic_die self_type;
		public:
			/* NOTE: we do *not* insert into the dieset in these constructors! 
			 * This is because make_shared won't set up ownership of the shared
			 * pointer until the object itself is fully constructed. If we try
			 * to make a shared pointer in the constructor, we will end up with
			 * two counts for a single object, and it will get deleted when the
			 * first one of thes hits zero. This should not happen! Instead, it's
			 * the factory's responsibility to add stuff to the dieset. */
			// special constructor used by all_compile_units
			basic_die(encap::dieset& ds, Dwarf_Off parent, Dwarf_Half tag, 
				Dwarf_Off offset, Dwarf_Off cu_offset, 
				const encap::die::attribute_map& attrs, 
				const set<Dwarf_Off>& children)
			 :	encap::die(ds, parent, tag, offset, cu_offset, attrs, children) 
			 { cu_offset = 0;}
			// "encap" constructor
		protected:
			basic_die(encap::dieset& ds, lib::die& d, Dwarf_Off parent_off)
			 :  encap::die(ds, d, parent_off) 
			 { cu_offset = 0; }
		public:
			basic_die(Dwarf_Half tag, encap::dieset& ds, shared_ptr<lib::die> p_d, Dwarf_Off parent_off)
			 :  encap::die(ds, p_d, parent_off)
			{ Dwarf_Half t; p_d->tag(&t); assert(t == tag); cu_offset = 0; }
			// "create" constructor
			basic_die(Dwarf_Half tag,
				self& parent, 
				opt<const std::string&> name)
			 :	encap::die(parent.m_ds, parent.m_offset, tag, parent.m_ds.next_free_offset(), 
			 	0, encap::die::attribute_map(), set<Dwarf_Off>())
				// FIXME: don't pass 0 as cu_offset
			{
				cu_offset = 0;
				if (name) m_attrs.insert(std::make_pair(DW_AT_name, dwarf::encap::attribute_value(
					parent.m_ds, std::string(*name))));
			}
// 			virtual ~basic_die()
// 			{ 	std::cerr << "Destructing encap::basic_die.";
// 				void *array[10];
// 				size_t size;
// 				char **strings;
// 				size_t i;
// 
// 				size = backtrace (array, 10);
// 				strings = backtrace_symbols (array, size);
// 
// 				printf ("Obtained %zd stack frames.\n", size);
// 
// 				for (i = 0; i < size; i++) std::cerr << strings[i] << std::endl;
// 
// 				free (strings); 
// 			}
			
			const dwarf::spec::abstract_def& get_spec() { return get_ds().get_spec(); }		   
			 
			attr_optional(name, string);

			// manually defined because they don't map to DWARF attributes
			Dwarf_Off get_offset() const { return m_offset; }
			std::shared_ptr<self> get_parent() const 
			{ 
				//auto found = m_ds.map_find(m_parent); 
				//assert(found != m_ds.map_end()); 
				//return dynamic_pointer_cast<basic_die>(found->second); 
				return dynamic_pointer_cast<encap::basic_die>(p_parent);
			}
			Dwarf_Half get_tag() const { return m_tag; }
			
			bool integrity_check()
			{
				bool retval = true;
				auto p_seq = this->all_refs_dfs_seq();
				for (auto i = p_seq->begin(/*p_seq*/); i != p_seq->end(/*p_seq*/); i++)
				{
					Dwarf_Off target = i->second.get_ref().off;

					// bool abs = i->second.get_ref().abs;
					// assert(abs);
					
					bool is_valid = this->get_ds().map_find(target) != this->get_ds().map_end();
					retval &= is_valid;
					if (!is_valid)
					{
						std::cerr << "Warning: referential integrity violation in dieset: "
							<< "attribute " 
							<< this->get_ds().get_spec().attr_lookup(i->first)
							<< " refers to nonexistent DIE offset 0x" << std::hex << target
							<< " in " 
							<< *dynamic_pointer_cast<encap::die, spec::basic_die>(
								this->get_ds()[i->second.get_ref().referencing_off]
								)
							<< std::endl;
					}
				}
				return retval;
			}
		};

#include "dwarf3-adt.h"
		class file_toplevel_die : public basic_die, public virtual spec::file_toplevel_die
		{
			friend class dieset;
		public:
			/* special constructor */ 
			file_toplevel_die(dieset& ds) 
			 :	basic_die(ds, 0UL, 0, 0UL, 0UL, 
			 	encap::die::attribute_map(), set<Dwarf_Off>()) {} 

			child_tag(compile_unit)
		};

#undef extra_decls_subprogram
#undef extra_decls_compile_unit

#undef forward_decl
#undef declare_base
#undef declare_bases
#undef base_fragment
#undef initialize_base
#undef constructor
#undef begin_class
#undef base_initializations
#undef end_class
#undef stored_type_string
#undef stored_type_flag
#undef stored_type_unsigned
#undef stored_type_signed
#undef stored_type_offset
#undef stored_type_half
#undef stored_type_ref
#undef stored_type_tag
#undef stored_type_loclist
#undef stored_type_address
#undef stored_type_refdie
#undef stored_type_refdie_is_type
#undef stored_type_rangelist
#undef attr_optional
#undef attr_mandatory
#undef super_attr_optional
#undef super_attr_mandatory
#undef child_tag

/****************************************************************/
/* end generated ADT includes								   */
/****************************************************************/


		// encap factory
		class factory //: public abstract::factory
		{
			//friend class dwarf::abstract::factory;
			friend class dwarf::encap::file;
			static encap::factory *const dwarf3_factory;
		public:
			// convenience forwarder
			static factory& for_spec(const dwarf::spec::abstract_def& spec); 
		protected:
			void attach_to_ds(std::shared_ptr<basic_die> p) const
			{ 
				assert(p->get_ds().find(p->parent_offset()) != p->get_ds().end());
				dynamic_pointer_cast<encap::basic_die>(p->get_ds()[p->parent_offset()])
					->attach_child(p);
			}
			template<typename T, typename... Args > 
			std::shared_ptr<encap::basic_die>
			my_make_shared(Args&&... args) const
			{ std::shared_ptr<encap::basic_die> p(new T(std::forward<Args>(args)...)); return p; }
			virtual std::shared_ptr<die> encapsulate_die(Dwarf_Half tag, 
				dieset& ds, lib::die& d, Dwarf_Off parent_off) const = 0;
		public:
			virtual 
			std::shared_ptr<basic_die> 
			create_die(Dwarf_Half tag, shared_ptr<basic_die> parent,
				opt<std::string> name 
					= opt<std::string>()) const = 0;
			virtual
			std::shared_ptr<basic_die>
			clone_die(dieset& dest_ds, std::shared_ptr<basic_die> p_d) const = 0;
		};
	} // end namespace encap
} // end namespace dwarf

#endif

