/*! Defines some metaprogramming utilities.
 * \file
 *
 * \todo
 * - complete documentation
 */

/*----------------------------------------------------------------------------*/

#ifndef _PASLEXAMPLES_TYPELIST_H_
#define _PASLEXAMPLES_TYPELIST_H_

#include <type_traits>
#include <cstddef>
#include <utility>

namespace util {
  /*--------------------------------------------------------------------------*/
  /* Metaprogramming helper types */
   
  /*! Dummy type whose sole purpose is to hold a list of types.
   *
   * \tparam T the parameter pack containing the types
   */
  template<class... T>
  struct type_list {};
  
  /*! Dummy type whose sole purpose is to hold a list of integers.
   *
   * This type is quite similar to C++14's std::integer_sequence.
   *
   * \tparam T   the type of the integer
   * \tparam Seq the integers
   */
  template<class T, T... Seq>
  struct int_list {
    static_assert(std::is_integral<T>::value, "T should be an integral type");
  };
  
  namespace _detail {
    template<class...>
    struct void_type {
      typedef void type;
    };
  } // namespace _detail
  
  /*! Always void. Useful for SFINAE.
   *
   * \tparam T dummy parameters (whether they can be built or not will allow or
   *           reject a specialization where void_t appears)
   */
  template<class... T>
  using void_t = typename _detail::void_type<T...>::type;
  
  /*! Always false. Useful for static assertions in forbidden specializations.
   *
   * \tparam T dummy parameters so that the compiler doesn't eagerly stop
   *           because of a false static_assert
   */
  template<class... T>
  struct false_predicate {
    static constexpr bool value = false;
  };
  
  /*--------------------------------------------------------------------------*/
  /* Operations on compile-time list types */
  
  /*! Metafunction "curryfication".
   *
   * \tparam Fct the metafunction to be curried
   * \tparam T   its first template argument
   */
  template<template<class...> class Fct, class T>
  struct curry {
    template<class... Args>
    using curried = Fct<T, Args...>;
  };
  
  namespace _detail {
    template<class S1, class S2>
    struct cat;
    
    template<class T, T... Seq1, T... Seq2>
    struct cat<int_list<T, Seq1...>, int_list<T, Seq2...>> {
      typedef int_list<T, Seq1..., Seq2...> type;
    };
    
    template<class... Seq1, class... Seq2>
    struct cat<type_list<Seq1...>, type_list<Seq2...>> {
      typedef type_list<Seq1..., Seq2...> type;
    };
    
    template<class S, std::size_t lo, std::size_t hi, class = void>
    struct sub;
    
    template<class T, T x, T... Seq, std::size_t lo, std::size_t hi>
    struct sub<
    int_list<T, x, Seq...>, lo, hi,
    typename std::enable_if<(lo > 0 && hi > 0)>::type
    > :
    sub<int_list<T, Seq...>, lo - 1, hi - 1> {};
    
    template<class T, T x, T... Seq, std::size_t hi>
    struct sub<
    int_list<T, x, Seq...>, 0, hi,
    typename std::enable_if<(hi > 0)>::type
    > :
    cat<int_list<T, x>, typename sub<int_list<T, Seq...>, 0, hi - 1>::type> {};
    
    template<class T, T... Seq>
    struct sub<int_list<T, Seq...>, 0, 0, void> {
      typedef int_list<T> type;
    };
    
    template<class T, class... Seq, std::size_t lo, std::size_t hi>
    struct sub<
    type_list<T, Seq...>, lo, hi,
    typename std::enable_if<(lo > 0 && hi > 0)>::type
    > :
    sub<type_list<Seq...>, lo - 1, hi - 1> {};
    
    template<class T, class... Seq, std::size_t hi>
    struct sub<
    type_list<T, Seq...>, 0, hi,
    typename std::enable_if<(hi > 0)>::type
    > :
    cat<type_list<T>, typename sub<type_list<Seq...>, 0, hi - 1>::type> {};
    
    template<class... Seq>
    struct sub<type_list<Seq...>, 0, 0, void> {
      typedef type_list<> type;
    };
    
    template<class T, T lo, T hi>
    struct make {
      static_assert(std::is_integral<T>::value, "T should be an integral type");
      static_assert(lo <= hi, "lo > hi!");
      typedef typename cat<
                         int_list<T, lo>, typename make<T, lo + 1, hi>::type
                       >::type type;
    };
    
    template<class T, T lo>
    struct make<T, lo, lo> {
      typedef int_list<T> type;
    };
    
    template<std::size_t Idx, class S>
    struct get_template {};
    
    template<std::size_t Idx, class T, T Elt, T... Seq>
    struct get_template<Idx, int_list<T, Elt, Seq...>> :
    get_template<Idx - 1, int_list<T, Seq...>> {};
    
    template<class T, T Elt, T... Seq>
    struct get_template<0, int_list<T, Elt, Seq...>> {
      static const T value;
    };
    
    template<std::size_t Idx, class T>
    struct get_template<Idx, int_list<T>> {};
    
    template<class T, T Elt, T... Seq>
    const T get_template<0, int_list<T, Elt, Seq...>>::value = Elt;
    
    template<std::size_t Idx, class T, class... Seq>
    struct get_template<Idx, type_list<T, Seq...>> :
    get_template<Idx - 1, type_list<Seq...>> {};
    
    template<class T, class... Seq>
    struct get_template<0, type_list<T, Seq...>> {
      typedef T type;
    };
    
    template<std::size_t Idx>
    struct get_template<Idx, type_list<>> {};
    
    template<template<class...> class Predicate, class Seq>
    struct split;
    
    template<template<class...> class Predicate, class T, class... Seq>
    struct split<Predicate, type_list<T, Seq...>> {
    private:
      typedef split<Predicate, type_list<Seq...>> parent_split;
      typedef typename parent_split::first_type parent_first_type;
      typedef typename parent_split::second_type parent_second_type;
    public:
      typedef typename std::conditional<
                         Predicate<T>::value,
                         typename cat<type_list<T>, parent_first_type>::type,
                         parent_first_type
                       >::type first_type;
      typedef typename std::conditional<
                         !Predicate<T>::value,
                         typename cat<type_list<T>, parent_second_type>::type,
                         parent_second_type
                       >::type second_type;
    };
    
    template<template<class...> class Predicate>
    struct split<Predicate, type_list<>> {
      typedef type_list<> first_type;
      typedef type_list<> second_type;
    };
    
    template<template<class...> class Predicate, class Seq>
    struct filter {
      typedef typename split<Predicate, Seq>::first_type type;
    };
    
    template<template<class...> class Predicate, class Seq>
    struct filter_not {
      typedef typename split<Predicate, Seq>::second_type type;
    };
    
    template<class Seq>
    struct unique;
    
    template<class T, class... Seq>
    struct unique<type_list<T, Seq...>> {
      typedef typename cat<
                         type_list<T>,
                         typename unique<
                           typename filter_not<
                             curry<std::is_same, T>::template curried,
                             type_list<Seq...>
                           >::type
                         >::type
                       >::type type;
    };
    
    template<>
    struct unique<type_list<>> {
      typedef type_list<> type;
    };
    
    template<template<class...> class Predicate, class Seq, class = void>
    struct find {
      static_assert(util::false_predicate<Seq>::value,
                    "find_t should be used with a type_list");
    };
    
    template<template<class...> class Predicate, class T, class... Seq>
    struct find<
             Predicate, type_list<T, Seq...>,
             typename std::enable_if<Predicate<T>::value>::type
           > {
      typedef T type;
    };
    
    template<template<class...> class Predicate, class T, class... Seq>
    struct find<
             Predicate, type_list<T, Seq...>,
             typename std::enable_if<!Predicate<T>::value>::type
           > : find<Predicate, type_list<Seq...>> {};
    
    template<template<class...> class Predicate>
    struct find<Predicate, type_list<>, void> {};
    
    template<
      template<class...> class Predicate, class Seq, class Default,
      class = void
    >
    struct find_or_default {
      typedef Default type;
    };
    
    template<template<class...> class Predicate, class Seq, class Default>
    struct find_or_default<
             Predicate, Seq, Default,
             util::void_t<typename find<Predicate, Seq>::type>
           > {
      typedef typename find<Predicate, Seq>::type type;
    };
  } // namespace _detail
  
  /*! Concatenates either two \c type_list or two \c int_list.
   *
   * \tparam S1 the type of the first list
   * \tparam S2 the type of the second list
   */
  template<class S1, class S2>
  using cat_t = typename _detail::cat<S1, S2>::type;
  
  /*! Creates a sublist of a \c type_list or an \c int_list.
   *
   * \tparam S  the type of the list
   * \tparam lo the starting index (included)
   * \tparam hi the ending index (excluded)
   */
  template<class S, std::size_t lo, std::size_t hi>
  using sub_t = typename _detail::sub<S, lo, hi>::type;
  
  /*! Creates an int_list containing [\c lo ; \c hi ).
   *
   * \tparam T  the integer's type
   * \tparam lo the lower bound (included)
   * \tparam hi the upper bound (excluded)
   */
  template<class T, T lo, T hi>
  using make_t = typename _detail::make<T, lo, hi>::type;
  
  //c++11 doesn't allow using get_template<n, S>::value
  /*! Defines either the nth type of a type_list or the nth integer of an
   *  int_list.
   */
  template<std::size_t n, class S>
  using get_template = _detail::get_template<n, S>;
  
  /*! Gets the nth type of a \c type_list.
   * 
   * \tparam n the type's index
   * \tparam S the type_list's type
   */
  template<std::size_t n, class S>
  using get_template_t = typename _detail::get_template<n, S>::type;
  
  /*! Filters the types of a \c type_list.
   *
   * The resulting type_list contains the types of the given \c type_list which
   * satisfy the predicate.
   *
   * \tparam Predicate a template template parameter taking a type as parameter
   *                   and defining a static constexpr boolean named \c value
   * \tparam Seq       the type of the type_list
   */
  template<template<class> class Predicate, class Seq>
  using filter_t = typename _detail::filter<Predicate, Seq>::type;
  
  /*! Filters the types of a \c type_list.
   *
   * The resulting type_list contains the types of the given \c type_list which
   * don't satisfy the predicate.
   *
   * \tparam Predicate a template template parameter taking a type as parameter
   *                   and defining a static constexpr boolean named \c value
   * \tparam Seq       the type of the type_list
   */
  template<template<class> class Predicate, class Seq>
  using filter_not_t = typename _detail::filter_not<Predicate, Seq>::type;
  
  /*! Creates a new \c type_list containing the same types as the given
   *  \c type_list without duplicates.
   *
   * Note: sub-optimal compile-time complexity! (square instead of quasi-linear)
   *
   * \tparam Seq the type of the type_list
   */
  template<class Seq>
  using unique_t = typename _detail::unique<Seq>::type;
  
  /*! First type of a type_list which satisfies the given predicate, compilation
   *  error if no such type exists.
   *
   * Note: linear compile-time complexity
   *
   * \tparam Predicate a template template parameter taking a type as parameter
   *                   and defining a static constexpr boolean named \c value
   * \tparam Seq       the type of the type_list
   */
  template<template<class...> class Predicate, class Seq>
  using find_t = typename _detail::find<Predicate, Seq>::type;
  
  /*! First type of a type_list which satisfies the given predicate, or Default
   *  if no such type exists.
   *
   * Note: linear compile-time complexity
   *
   * \tparam Predicate a template template parameter taking a type as parameter
   *                   and defining a static constexpr boolean named \c value
   * \tparam Seq       the type of the type_list
   * \tparam Default   the default type;
   */
  template<template<class...> class Predicate, class Seq, class Default>
  using find_or_default_t =
  typename _detail::find_or_default<Predicate, Seq, Default>::type;
  
  /*--------------------------------------------------------------------------*/
  /* Argument filtering */
  
  
  namespace _detail {
    template<std::size_t n, class T, class... Args>
    struct get_arg_helper {
      // use std::forward insted of the static_cast in c++14
      static constexpr auto call(T&&, Args&&... args) noexcept ->
      decltype(get_arg_helper<n - 1, Args...>::
               call(static_cast<Args&&>(args)...)) {
        return get_arg_helper<n - 1, Args...>::
               call(static_cast<Args&&>(args)...);
      }
    };
    
    template<class T, class... Args>
    struct get_arg_helper<0, T, Args...> {
      // same here
      static constexpr T&& call(T&& arg, Args&&...) noexcept {
        return static_cast<T&&>(arg);
      }
    };
  } // namespace _detail
  
  /*! Returns the nth argument.
   *
   * \tparam n    the argument's index
   * \tparam Args parameter pack holding the arguments' types
   */
  template<std::size_t n, class... Args>
  constexpr auto get_arg(Args&&... args) noexcept ->
  decltype(_detail::get_arg_helper<n, Args...>::
           call(std::forward<Args>(args)...)) {
    return _detail::get_arg_helper<n, Args...>::
           call(std::forward<Args>(args)...);
  }
  
  /*! Given a function, a list of arguments, and a compile-time list of
   *  positions, call the function with the arguments at the aforementioned
   *  positions.
   *
   * \tparam Pos  the positions of the arguments you want to pass to the
   *              function, they must be explicitly specified
   * \tparam Fct  the function's type
   * \tparam Args the arguments' types
   *
   * \param fct  the function
   * \param args the argument list
   */
  template<std::size_t... Pos, class Fct, class... Args>
  auto filter_args(Fct&& fct, Args&&... args) -> 
  decltype(std::forward<Fct>(fct)(get_arg<Pos>(std::forward<Args>(args)...)...)) {
    return std::forward<Fct>(fct)(get_arg<Pos>(std::forward<Args>(args)...)...);
  }
  
  namespace _detail {
    template<class Seq, class Fct, class... Args>
    struct filter_args_helper;
    
    template<std::size_t... Pos, class Fct, class... Args>  
    struct filter_args_helper<int_list<std::size_t, Pos...>, Fct, Args...> {
      static auto call(Fct&& fct, Args&&... args) ->
      decltype(filter_args<Pos...>(std::forward<Fct>(fct),
                                   std::forward<Args>(args)...)) {
        return filter_args<Pos...>(std::forward<Fct>(fct),
                                   std::forward<Args>(args)...);
      }
    };
  } // namespace _detail
  
  /*! Given a function, a list of arguments, and a compile-time list of
   *  positions, call the function with the arguments at the aforementioned
   *  positions.
   *
   * \tparam Seq  an int_list containing the positions of the arguments you
   *              want to pass to the function, it must be explicitly specified
   * \tparam Fct  the function's type
   * \tparam Args the arguments' types
   *
   * \param fct  the function
   * \param args the argument list
   */
  template<class Seq, class Fct, class... Args>
  auto filter_args(Fct&& fct, Args&&... args) -> 
  decltype(_detail::filter_args_helper<Seq, Fct, Args...>::
           call(std::forward<Fct>(fct), std::forward<Args>(args)...)) {
    return _detail::filter_args_helper<Seq, Fct, Args...>::
           call(std::forward<Fct>(fct), std::forward<Args>(args)...);
  }
  
} // namespace util

#endif // _PASLEXAMPLES_TYPELIST_H_
