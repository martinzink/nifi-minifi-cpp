// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

use proc_macro::TokenStream;
use proc_macro2::TokenStream as TokenStream2;
use quote::quote;
use syn::{Data, DeriveInput, Fields, ItemTrait, Meta, parse_macro_input};

#[proc_macro_derive(ComponentIdentifier)]
pub fn derive_component_identifier(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let name = &input.ident;
    let name_str = name.to_string();

    let expanded = quote! {
        impl ::minifi_native::ComponentIdentifier for #name {
            const CLASS_NAME: &'static str = concat!(module_path!(), "::", #name_str);
            const GROUP_NAME: &'static str = env!("CARGO_PKG_NAME");
            const VERSION: &'static str = env!("CARGO_PKG_VERSION");
        }

        impl ::minifi_native::PropertySchema for #name {
            const CONSTRAINT: Option<::minifi_native::PropertyConstraints> =
                Some(::minifi_native::PropertyConstraints::ControllerService(<Self as ::minifi_native::ComponentIdentifier>::CLASS_NAME));
            const IS_REQUIRED: bool = true;
        }
    };

    TokenStream::from(expanded)
}

/// Declares how a single-threaded processor's state maps onto the state store.
///
/// `#[persisted]` round-trips a field through one entry keyed by the field name (via `StateValue`);
/// `#[persisted(flatten)]` hands it the whole map so it can own a group of entries (via `StateEntries`).
#[proc_macro_derive(StateSchema, attributes(persisted))]
pub fn derive_state_schema(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    match state_schema_impl(&input) {
        Ok(tokens) => TokenStream::from(tokens),
        Err(err) => err.to_compile_error().into(),
    }
}

enum PersistedKind {
    Value,
    Flatten,
}

fn state_schema_impl(input: &DeriveInput) -> Result<TokenStream2, syn::Error> {
    let name = &input.ident;

    // Anything without named fields simply has no state, so give it the no-op impl rather than rejecting it.
    let fields = match &input.data {
        Data::Struct(data) => match &data.fields {
            Fields::Named(named) => Some(&named.named),
            _ => None,
        },
        _ => None,
    };

    let mut persisted = Vec::new();
    for field in fields.into_iter().flatten() {
        let Some(attr) = field.attrs.iter().find(|a| a.path().is_ident("persisted")) else {
            continue;
        };
        let kind = match &attr.meta {
            Meta::Path(_) => PersistedKind::Value,
            Meta::List(list) => {
                let nested: syn::Ident = list.parse_args()?;
                if nested == "flatten" {
                    PersistedKind::Flatten
                } else {
                    return Err(syn::Error::new_spanned(
                        &nested,
                        "unknown `persisted` option; the only supported one is `flatten`",
                    ));
                }
            }
            Meta::NameValue(value) => {
                return Err(syn::Error::new_spanned(
                    value,
                    "`persisted` takes no value; write `#[persisted]` or `#[persisted(flatten)]`",
                ));
            }
        };
        let ident = field
            .ident
            .as_ref()
            .expect("named field has an identifier")
            .clone();
        persisted.push((ident, kind));
    }

    let has_state = !persisted.is_empty();

    let loads = persisted.iter().map(|(ident, kind)| match kind {
        PersistedKind::Value => {
            let key = ident.to_string();
            quote! {
                if let Some(raw) = state.get(#key) {
                    self.#ident = ::minifi_native::StateValue::from_state_string(raw).map_err(|err| {
                        ::minifi_native::MinifiError::validation(format!(
                            "could not restore persisted state field '{}': {}", #key, err
                        ))
                    })?;
                }
            }
        }
        PersistedKind::Flatten => quote! {
            ::minifi_native::StateEntries::load_entries(&mut self.#ident, state)?;
        },
    });

    let stores = persisted.iter().map(|(ident, kind)| match kind {
        PersistedKind::Value => {
            let key = ident.to_string();
            quote! {
                state.insert(
                    ::std::string::String::from(#key),
                    ::minifi_native::StateValue::to_state_string(&self.#ident).map_err(|err| {
                        ::minifi_native::MinifiError::validation(format!(
                            "could not store persisted state field '{}': {}", #key, err
                        ))
                    })?,
                );
            }
        }
        PersistedKind::Flatten => quote! {
            ::minifi_native::StateEntries::store_entries(&self.#ident, state)?;
        },
    });

    let state_param = if has_state {
        quote!(state)
    } else {
        quote!(_state)
    };

    Ok(quote! {
        impl ::minifi_native::StateSchema for #name {
            const HAS_STATE: bool = #has_state;

            fn load_from_state(
                &mut self,
                #state_param: &::std::collections::HashMap<::std::string::String, ::std::string::String>,
            ) -> ::core::result::Result<(), ::minifi_native::MinifiError> {
                #(#loads)*
                ::core::result::Result::Ok(())
            }

            fn store_to_state(
                &self,
                #state_param: &mut ::std::collections::HashMap<::std::string::String, ::std::string::String>,
            ) -> ::core::result::Result<(), ::minifi_native::MinifiError> {
                #(#stores)*
                ::core::result::Result::Ok(())
            }
        }
    })
}

#[proc_macro_attribute]
pub fn controller_service_api(_attr: TokenStream, item: TokenStream) -> TokenStream {
    let input = parse_macro_input!(item as ItemTrait);
    let name = &input.ident;
    let name_str = name.to_string();

    let expanded = quote! {
        #input

        impl ::minifi_native::ControllerServiceApi for dyn #name {
            const INTERFACE_NAME: &'static str = concat!(module_path!(), "::", #name_str);
        }

        impl ::minifi_native::PropertySchema for dyn #name {
            const CONSTRAINT: Option<::minifi_native::PropertyConstraints> =
                Some(::minifi_native::PropertyConstraints::ControllerService(<Self as ::minifi_native::ControllerServiceApi>::INTERFACE_NAME));
            const IS_REQUIRED: bool = true;
        }
    };

    TokenStream::from(expanded)
}

#[proc_macro_derive(PropertyType)]
pub fn derive_property_type(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let name = &input.ident;

    let expanded = quote! {
        impl ::minifi_native::PropertyType for #name {
            type Output = #name;
            fn parse(s: &str) -> Result<Self::Output, ::minifi_native::MinifiError> {
                s.parse::<#name>().map_err(Into::into)
            }
        }

        impl ::minifi_native::PropertySchema for #name {
            const CONSTRAINT: Option<::minifi_native::PropertyConstraints> =
                Some(::minifi_native::PropertyConstraints::AllowedValues(
                    <#name as ::strum::VariantNames>::VARIANTS
                ));
            const IS_REQUIRED: bool = true;
        }
    };

    TokenStream::from(expanded)
}
