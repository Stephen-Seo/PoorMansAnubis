// ISC License
//
// Copyright (c) 2025 Stephen Seo
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

use std::sync::Arc;

use futures::{StreamExt, TryStreamExt, stream::FuturesUnordered};
use salvo::{
    Listener,
    conn::{Acceptor, Holding, TcpListener, tcp::TcpAcceptor},
    fuse::FuseFactory,
};
use tokio::net::ToSocketAddrs;

pub struct TcpVectorAcceptor {
    acceptors: Vec<TcpAcceptor>,
    holdings: Vec<Holding>,
}

impl TcpVectorAcceptor {
    fn new() -> Self {
        Self {
            acceptors: Vec::new(),
            holdings: Vec::new(),
        }
    }

    fn finalize_holdings(&mut self) {
        self.holdings = self
            .acceptors
            .iter()
            .map(|a| a.holdings())
            .collect::<Vec<&[Holding]>>()
            .concat();
    }
}

impl Acceptor for TcpVectorAcceptor {
    type Coupler = <TcpAcceptor as Acceptor>::Coupler;
    type Stream = <TcpAcceptor as Acceptor>::Stream;

    fn holdings(&self) -> &[Holding] {
        &self.holdings
    }

    async fn accept(
        &mut self,
        fuse_factory: Option<Arc<dyn FuseFactory + Sync + Send + 'static>>,
    ) -> std::io::Result<salvo::conn::Accepted<Self::Coupler, Self::Stream>> {
        let iter = self.acceptors.iter_mut();
        let futures = FuturesUnordered::from_iter(iter.map(|a| a.accept(fuse_factory.clone())));

        futures
            .try_ready_chunks(1)
            .next()
            .await
            .ok_or(std::io::Error::other("accept on TcpVectorAcceptor Failed"))?
            .map_err(|e| e.1)?
            .into_iter()
            .next()
            .ok_or(std::io::Error::other("accept on TcpVectorAcceptor Failed"))
    }
}

pub struct TcpVectorListener<T> {
    listeners: Vec<TcpListener<T>>,
}

impl<T> TcpVectorListener<T>
where
    T: ToSocketAddrs + Send,
{
    pub fn new() -> Self {
        Self {
            listeners: Vec::new(),
        }
    }

    pub fn push(&mut self, listener: TcpListener<T>) {
        self.listeners.push(listener);
    }
}

impl<T> Listener for TcpVectorListener<T>
where
    T: ToSocketAddrs + Send + 'static,
{
    type Acceptor = TcpVectorAcceptor;

    async fn try_bind(self) -> salvo::core::Result<Self::Acceptor> {
        let mut v_acceptor = TcpVectorAcceptor::new();

        for listener in self.listeners.into_iter() {
            v_acceptor.acceptors.push(listener.try_bind().await?);
        }

        v_acceptor.finalize_holdings();

        Ok(v_acceptor)
    }
}
