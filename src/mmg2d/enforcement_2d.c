/* =============================================================================
**  This file is part of the mmg software package for the tetrahedral
**  mesh modification.
**  Copyright (c) Bx INP/Inria/UBordeaux/UPMC, 2004- .
**
**  mmg is free software: you can redistribute it and/or modify it
**  under the terms of the GNU Lesser General Public License as published
**  by the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  mmg is distributed in the hope that it will be useful, but WITHOUT
**  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
**  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
**  License for more details.
**
**  You should have received a copy of the GNU Lesser General Public
**  License and of the GNU General Public License along with mmg (in
**  files COPYING.LESSER and COPYING). If not, see
**  <http://www.gnu.org/licenses/>. Please read their terms carefully and
**  use this copy of the mmg distribution only if you accept them.
** =============================================================================
*/
#include "mmg2d.h"


/**
 * \param mesh pointer toward the mesh structure.
 * \param sol pointer toward the sol structure.
 * \return 0 if fail, 1 otherwise.
 *
 * Check if all edges exist in the mesh and if not force them.
 *
 */
int MMG2_bdryenforcement(MMG5_pMesh mesh,MMG5_pSol sol) {
  MMG5_pTria      pt,pt1;
  MMG5_pEdge      ped;
  MMG5_pPoint     ppt;
  int             k,l,kk,nex,list[MMG2_LONMAX],kdep,lon,voy,iel,iare,ied;
  int             ia,ib,ilon,rnd,idep,*adja,ir,adj,list2[3];
  char            i,i1,i2,j;
//  int       iadr2,*adja2,ndel,iadr,ped0,ped1;

  nex = 0;
  
  /* Associate seed to each point in the mesh */
  for (k=1; k<=mesh->np; k++) {
    ppt = &mesh->point[k];
    if ( MG_VOK(ppt) ) ppt->s = 0;
  }
  
  for (k=1; k<=mesh->nt; k++) {
    pt = &mesh->tria[k];
    if ( !MG_EOK(pt) ) continue;
    for (i=0; i<3; i++)
      mesh->point[pt->v[i]].s = k;
  }
  
  /* Check for the existing edges in the mesh */
  /* No command for identifying whether an edge is valid or not? */
  for(k=1; k<=mesh->na; k++) {
    ped = &mesh->edge[k];
    if ( !ped->a ) continue;
    if ( ped->base < 0 ) {
      nex++;
      continue;
    }
    
    ppt = &mesh->point[ped->a];
    kdep = ppt->s;
    pt = &mesh->tria[kdep];
    
    if ( pt->v[0] == ped->a )
      j=0;
    else if ( pt->v[1] == ped->a )
      j=1;
    else
      j=2;
    
    lon = _MMG2_boulet(mesh,kdep,j,list);
    
    if ( lon <= 0 ) {
      printf(" ## Error: problem with point %d of triangle %d\n",mesh->tria[kdep].v[j],kdep);
      return(0);
    }
    
    for (l=0; l<lon; l++) {
      iel = list[l] / 3;
      pt  = &mesh->tria[iel];
      i = list[l] % 3;
      i1 = _MMG5_inxt2[i];
      i2 = _MMG5_iprv2[i];
      
      if ( pt->v[i1] == ped->b ) {
        ped->base = -1;
        nex++;
        break;
      }
      else if ( pt->v[i2] == ped->b ) {
        ped->base = -1;
        nex++;
        break;
      }
    }
    
    if ( l >= lon ) {
      if(mesh->info.imprim > 5) printf("  ** missing edge %d %d \n",ped->a,ped->b);
      ped->base = kdep;
    }
  }
    
  /* Now treat the missing edges */
  if ( nex != mesh->na ) {
    if(mesh->info.imprim > 5)
      printf(" ** number of missing edges : %d\n",mesh->na-nex);
    
    for (kk=1; kk<=mesh->na; kk++) {
      ped = &mesh->edge[kk];
      if ( !ped->a || ped->base < 0 ) continue;
      ia = ped->a;
      ib = ped->b;
      kdep = ped->base;
      
      if(mesh->info.ddebug)
        printf("\n  -- edge enforcement %d %d\n",ia,ib);

      if ( !(lon=MMG2_locateEdge(mesh,ia,ib,&kdep,list)) ) {
        if(mesh->info.ddebug)
          printf("  ## Error: edge not found\n");
        return(0);
      }
      
      if ( !( lon < 0 || lon == 4 ) ) {
        if(mesh->info.ddebug)
          printf("  ** Unexpected situation: edge %d %d -- %d\n",ia,ib,lon);
        exit(EXIT_FAILURE);
      }
      
      /* Considered edge actually exists */
      if ( lon == 4 ) {
        if(mesh->info.ddebug) printf("  ** Existing edge\n");
        //exit(EXIT_FAILURE);
      }
      
      if ( -lon > 1000 ) {
        printf(" ## Error: too many triangles (%d)\n",lon);
        exit(EXIT_FAILURE);
      }
      if ( lon < 2 ) {
        if(mesh->info.ddebug) printf("  ** few edges... %d\n",lon);
        //exit(EXIT_FAILURE);
      }
      
      lon = -lon;
      ilon = lon;

      /* Randomly swap edges in the list, while... */
      srand(time(NULL));
      
      while ( ilon > 0 ) {
        rnd = ( rand() % lon );
        k   = list[rnd] / 3;

        /* Check for triangle k in the pipe until one triangle with base+1 is met (indicating that it is 
           crossed by the considered edge) */
        l = 0;
        while ( l++ < lon ) {
          pt = &mesh->tria[k];
          if ( pt->base == mesh->base+1 ) break;
          k = list[(++rnd)%lon] / 3;
        }
        
        assert ( l <= lon );
        idep = list[rnd] % 3;
        // if(mesh->info.ddebug) printf("i= %d < %d ? on demarre avec %d\n",i,lon+1,k);
        adja = &mesh->adja[3*(k-1)+1];
        
        for (i=0; i<3; i++) {
          ir = (idep+i) % 3;
          
          /* Check the adjacent triangle in the pipe */
          adj = adja[ir] / 3;
          voy = adja[ir] % 3;
          pt1 = &mesh->tria[adj];
          if ( pt1->base != (mesh->base+1) ) continue;
          
          /* Swap edge ir in triangle k, corresponding to a situation where both triangles are to base+1 */
          if ( !_MMG2_swapdelone(mesh,sol,k,ir,1e+4,list2) ) {
            if ( mesh->info.ddebug ) printf("  ## Warning: unable to swap\n");
            continue;
          }
          
          /* Is new triangle intersected by ia-ib ?? */
          for (ied=1; ied<3; ied++) {
            iare = MMG2_cutEdgeTriangle(mesh,list2[ied],ia,ib);
            if ( !iare ) { /*tr not in pipe*/
              ilon--;
              if ( mesh->info.ddebug )
                printf("  ## Warning: tr %d not intersected ==> %d\n",list2[ied],ilon);
              mesh->tria[list2[ied]].base = mesh->base;
            }
            else if ( iare < 0 ) {
              mesh->tria[list2[ied]].base = mesh->base;
              ilon -= 2;
            }
            else {
              if ( mesh->info.ddebug ) printf("  ** tr intersected %d \n",list2[ied]);
              mesh->tria[list2[ied]].base = mesh->base+1;
            }
          }
          break;
        }
     }
    }
  }
  
  /* Reset ->s field of vertices */
  for (k=1; k<=mesh->np; k++) {
    ppt = &mesh->point[k];
    if ( MG_VOK(ppt) ) ppt->s = 0;
  }

/*   //check if there are more bdry edges.. and delete tr      */
/* #warning a optimiser en mettant un pile et un while */
/*   //printf("tr 1 : %d %d %d\n",mesh->tria[1].v[0],mesh->tria[1].v[1],mesh->tria[1].v[2]);  */
/*   do { */
/*     ndel=0; */
/*     for(k=1 ; k<=mesh->nt ; k++) { */
/*       pt = &mesh->tria[k]; */
/*       if(!pt->v[0]) continue;  */
/*       iadr = 3*(k-1) + 1;                       */
/*       adja = &mesh->adja[iadr];  */

/*       for(i=0 ; i<3 ; i++)  { */
/*  if(adja[i]) continue; */
/*         ped0 = pt->v[MMG2_iare[i][0]]; */
/*         ped1 = pt->v[MMG2_iare[i][1]]; */
/*         for(j=1 ; j<=mesh->na ; j++) { */
/*           ped = &mesh->edge[j]; */
/*           if((ped->a == ped0 && ped->b==ped1) || (ped->b == ped0 && ped->a==ped1)) break;   */
/*         } */
/*         if(j<=mesh->na)  */
/*           continue; */
/*  else  */
/*    break; */
/*       } */
/*       if(i==3) continue;  */
  /*       fprintf(stdout,"WARNING BDRY EDGES MISSING : DO YOU HAVE DUPLICATED VERTEX ? %d %d %d\n", */
/*        pt->v[0],pt->v[1],pt->v[2]); */
/*       for(i=0 ; i<3 ; i++) {         */
/*  if(!adja[i]) continue; */
/*  iadr2 = 3*(adja[i]/3-1) + 1; */
/*  adja2 = &mesh->adja[iadr2]; */
/*  adja2[adja[i]%3] = 0; */
/*       } */
/*       MMG2_delElt(mesh,k);  */
/*       ndel++; */

/*     }           */
/*   } while(ndel); */

  return(1);
}