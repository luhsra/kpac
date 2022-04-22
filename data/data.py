import pandas as pd
from tabulate import tabulate

def pd2org(df):
    return tabulate(df, headers=df.columns, tablefmt='orgtbl')

class Measurement:
    def __init__(self, csv):
        self.df = pd.read_csv(csv, index_col='name')

    def __getitem__(self, key):
        return self.df[key]

    def relative(self):
        rel = pd.DataFrame()
        rel['mean'] = self['mean']
        rel['rstd'] = self['std'] / self['mean']
        rel['rmin'] = (1-self['min'] / self['mean'])
        rel['rmax'] = (self['max'] / self['mean']-1)

        return rel

class Build:
    def __init__(self, inst, auths):
        self.df = pd.read_csv(inst, index_col='name')
        auths = pd.read_csv(auths, index_col='name')
        if not auths['pac'].equals(auths['aut']):
            raise RuntimeException("pac != aut")
        self.df['auths'] = auths['pac']

    def __getitem__(self, key):
        return self.df[key]

class Analysis:
    def __init__(self, nopac, pac, build):
        self.nopac = nopac
        self.pac = pac
        self.b = build

    def overhead(self):
        oh = pd.DataFrame()
        oh['overhead'] = (self.pac['mean'] - self.nopac['mean']) \
            / self.nopac['mean'] * 100

        return oh

    def aut_per_sec(self):
        aut_s = pd.DataFrame()
        aut_s['aut_s'] = 1/self.pac['mean'] * self.b['auths']

        return aut_s

    def cycles(self, clock):
        cost = pd.DataFrame()
        slowdown = self.pac['mean']-self.nopac['mean']
        cost['cycles'] = slowdown/self.b['auths'] * clock

        return cost
