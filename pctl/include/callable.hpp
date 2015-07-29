/*! Defines traits classes for callable objects.
 * \file
 *
 * \todo
 * - doc
 */
#ifndef _PASLEXAMPLES_CALLABLE_H_
#define _PASLEXAMPLES_CALLABLE_H_

#include "typelist.hpp"

namespace util {
  namespace _detail {
    template<class ReturnType, class... Arguments>
    struct call_operator_traits_final {
      typedef ReturnType return_type;
      typedef type_list<Arguments...> argument_typelist;
      static constexpr std::size_t argument_count = sizeof...(Arguments);
      template<std::size_t Index>
      using argument_type = get_template_t<Index, argument_typelist>;
      static constexpr bool callable = true;
      // the following are default values which might be overridden
      static constexpr bool member = false;
      static constexpr bool const_member = false;
      static constexpr bool function_object = true;
      typedef ReturnType type(Arguments...);
      typedef ReturnType(*pointer)(Arguments...);
      typedef ReturnType(&reference)(Arguments...);
      typedef ReturnType(&&rvalue_reference)(Arguments...);
    };
    
    // default: empty
    template<class CallOperator, class = void>
    struct call_operator_traits_helper {
      static constexpr bool callable = false;
      static constexpr bool function_object = false;
    };
    
    // functor C: same as pointer to method C::operator()
    template<class CallOperator>
    struct call_operator_traits_helper<
             CallOperator, void_t<decltype(&CallOperator::operator())>
           > :
    call_operator_traits_helper<decltype(&CallOperator::operator()), void> {
      static constexpr bool function_object = true;
    };
    
    // rvalue reference to functor C: same as pointer to method C::operator()
    template<class CallOperator>
    struct call_operator_traits_helper<CallOperator&&, void> :
    call_operator_traits_helper<decltype(&CallOperator::operator())> {
      static constexpr bool function_object = true;
    };
    
    // reference to functor C: same as pointer to method C::operator()
    template<class CallOperator>
    struct call_operator_traits_helper<CallOperator&, void> :
    call_operator_traits_helper<decltype(&CallOperator::operator())> {
      static constexpr bool function_object = true;
    };
    
    // pointer to method
    template<class Class, class ReturnType, class... Arguments>
    struct call_operator_traits_helper<ReturnType(Class::*)(Arguments...)> :
    call_operator_traits_final<ReturnType, Arguments...> {
      static constexpr bool member = true;
      static constexpr bool function_object = false;
      typedef Class type;
      typedef Class* pointer;
      typedef Class& reference;
      typedef Class&& rvalue_reference;
    };
    
    template<class Class, class ReturnType, class... Arguments>
    struct call_operator_traits_helper<
             ReturnType(Class::*)(Arguments...) const
           > :
    call_operator_traits_final<ReturnType, Arguments...> {
      static constexpr bool member = true;
      static constexpr bool const_member = true;
      static constexpr bool function_object = false;
      typedef Class type;
      typedef Class* pointer;
      typedef Class& reference;
      typedef Class&& rvalue_reference;
    };
    
    // function type
    template<class ReturnType, class... Arguments>
    struct call_operator_traits_helper<ReturnType(Arguments...)> :
    call_operator_traits_final<ReturnType, Arguments...> {};
    
    // pointer to function
    template<class ReturnType, class... Arguments>
    struct call_operator_traits_helper<ReturnType(*)(Arguments...)> :
    call_operator_traits_final<ReturnType, Arguments...> {};
    
    // reference to function
    template<class ReturnType, class... Arguments>
    struct call_operator_traits_helper<ReturnType(&)(Arguments...)> :
    call_operator_traits_final<ReturnType, Arguments...> {};
    
    // rvalue reference to function
    template<class ReturnType, class... Arguments>
    struct call_operator_traits_helper<ReturnType(&&)(Arguments...)> :
    call_operator_traits_final<ReturnType, Arguments...> {};
  } // namespace _detail

  /**
   * \brief This type helps determining if an overload exists.
   */
  template<class Signature, Signature>
  struct signature_checker {};
  
  /**
   * \brief Provides various informations about a callable type.
   *
   * \tparam Callable a (supposedly) callable type
   */
  template<class Callable>
  struct callable_traits : _detail::call_operator_traits_helper<Callable> {};
} // namespace util

#endif // _PASLEXAMPLES_CALLABLE_H_
