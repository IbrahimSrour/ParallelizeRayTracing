
#include "viewport.h"
#include "sphere.h"
#include "ray.h"
#include "defs.h"
#include <fstream>
#include "xml.h"

#ifdef SRAY_MPI
#	include <mpi.h>
#endif
bool TViewport::SaveToTGAFile(char *filename, int size)
{
    ofstream imageFile(filename, ios_base::binary);
    if (!imageFile)
    	return false;

    imageFile.put(0).put(0);
    imageFile.put(2);

    imageFile.put(0).put(0);
    imageFile.put(0).put(0);
    imageFile.put(0);

    imageFile.put(0).put(0);
    imageFile.put(0).put(0);

    imageFile.put((unsigned char)(this->Width & 0x00FF)).put((unsigned char)((this->Width & 0xFF00) / 256));
    imageFile.put((unsigned char)(this->Height & 0x00FF)).put((unsigned char)((this->Height & 0xFF00) / 256));

    imageFile.put(24);
    imageFile.put(0);

	int segment, x, y, i;

#if defined(SRAY_MPI) && defined(SRAY_LB1)
	
	int *segarray = new int [size+1];
	int *offarray = new int [size];

	int rem = this->Height%size;

	segarray[0] = 0;
	for(i = 1; i <= size; i++)
	{
		if(i < rem)
			segarray[i] = this->Height/size + 1 + segarray[i-1];
		else
			segarray[i] = this->Height/size + segarray[i-1];
		offarray[i-1] = 0;
	}

	int count = 0;
	for(i = 0; count < this->Height; i = (i+1)%size)
	{
		y = segarray[i] + offarray[i]; 
		if(y >= segarray[i+1])
			continue;
		count++;
		offarray[i]++;
		for(x = 0; x < this->Width; x++)
		{
			segment = (y*this->Width + x) * 3;
	        imageFile.put(this->ImageMatrix[segment]).put(this->ImageMatrix[segment + 1]).put(this->ImageMatrix[segment + 2]);
		}
	}

	delete [] offarray;
	delete [] segarray;
#else
	for(y = 0; y < this->Height; y++)
		for(x = 0; x < this->Width; x++)
		{
			segment = (y*this->Width + x) * 3;
			imageFile.put(this->ImageMatrix[segment]).put(this->ImageMatrix[segment + 1]).put(this->ImageMatrix[segment + 2]);
		}
#endif

	imageFile.close();

	return true;
}

bool TViewport::Render(int size = -1, int rank = -1)
{
	int starty, endy, yincr = 1, cLoad = 0, ly;

	bool isParallel = false;

#ifdef SRAY_MPI
	isParallel = true;
#elif defined (SRAY_OPENMP)
	isParallel = true;
#endif

	if (isParallel)
	{
#ifdef SRAY_LB1
		yincr = size;
		starty = rank;
		endy = this->Height - 1;
#else	// SRAY_LB1
		starty = rank * this->Height / size;
		endy = (rank + 1) * this->Height / size - 1;
#endif	// SRAY_LB1
	}
	else
	{
		starty = 0;
		endy = this->Height - 1;
	}

	if(!AllocateMemory(starty, endy, yincr, rank, size))
		return false;

	ly = -1;
	for(int y = starty; y <= endy; y += yincr)
	{
#ifdef SRAY_MPI
#	ifdef SRAY_LB1
		ly++;		
#	else
		ly = y - starty;
#	endif
#else
		ly = y;
#endif

		for(int x = 0; x < this->Width; x++)
		{
	#ifdef LOAD_MEASUREMENT
			cLoad = 20;
	#endif
			TColor color(0.0f, 0.0f, 0.0f);
			float incr = ONE_OVER_SQRT_SRAY_SUPERSAMPLING_COEF;
			for (float fracx = (float)x ; fracx < x + 1.0f; fracx += incr )
			for (float fracy = (float)y ; fracy < y + 1.0f; fracy += incr )
			{
	#ifdef LOAD_MEASUREMENT
				cLoad += 2;
	#endif
				color += ONE_OVER_SRAY_SUPERSAMPLING_COEF * Scene->Raytrace(TRay(TPoint(fracx,
					fracy, SRAY_NEG_INFINITY), TVector(0.0f, 0.0f, 1.0f)), cLoad).Exposure();
			}

			this->SetColor(x, ly, color.sRGBEncode());
	#ifdef LOAD_MEASUREMENT
			cLoad += 6;
	#endif

	#ifdef LOAD_MEASUREMENT
			this->SetLoad(x, ly, cLoad);
	#endif
		}
	}
   return true;
}
//------------------------------------------------------------------------------
void TViewport::SetColor(int x, int y, TColor c)
{
	int segment = (y*Width + x) * 3;
	this->ImageMatrix[segment++] = (COLOR) min(c.GetB() * 255.0f, 255.0f);
	this->ImageMatrix[segment++] = (COLOR) min(c.GetG() * 255.0f, 255.0f);
	this->ImageMatrix[segment] = (COLOR) min(c.GetR() * 255.0f, 255.0f);
}
//------------------------------------------------------------------------------
bool TViewport::SaveConfigToFile(char *filename)
{
   ofstream configFile(filename);
   if (!configFile)
      return false;

   int i;

	configFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
	configFile << "<inputset filename=\"output.xml\">" << endl;
	configFile << "\t<image>" << endl;
	configFile << "\t\t<width>" << this->Width << "</width>" << endl;
	configFile << "\t\t<height>" << this->Height << "</height>" << endl;
	configFile << "\t\t<objects>" << endl;

   for (i = 0; i < this->Scene->Object.size(); i++)
   {
      switch (this->Scene->Object[i]->GetShape()->GetType())
      {
         case ST_SPHERE :
            configFile << "\t\t\t<object id=\"" << i+1 << "\" type=\"sphere\">" << endl;
            configFile << "\t\t\t\t<shape>" << endl;
            configFile << "\t\t\t\t\t<center>" << this->Scene->Object[i]->GetShape()->GetOrigin() << "</center>" << endl;
            configFile << "\t\t\t\t\t<radius>" << this->Scene->Object[i]->GetShape()->GetSize() << "</radius>" << endl;
            configFile << "\t\t\t\t</shape>" << endl;

            configFile << "\t\t\t\t<materialproperty> " << endl;
            configFile << "\t\t\t\t\t<ambient>" << this->Scene->Object[i]->GetMaterialProperty()->GetAmbient() << "</ambient>" << endl;
				configFile << "\t\t\t\t\t<diffusion>" << this->Scene->Object[i]->GetMaterialProperty()->GetDiffusion() << "</diffusion>" << endl;
				configFile << "\t\t\t\t\t<specular>" << this->Scene->Object[i]->GetMaterialProperty()->GetSpecular() << "</specular>" << endl;
				configFile << "\t\t\t\t\t<shininess>" << this->Scene->Object[i]->GetMaterialProperty()->GetShininess() << "</shininess>" << endl;
				configFile << "\t\t\t\t\t<emission>" << this->Scene->Object[i]->GetMaterialProperty()->GetEmission() << "</emission>" << endl;
				configFile << "\t\t\t\t\t<reflection>" << this->Scene->Object[i]->GetMaterialProperty()->GetReflection() << "</reflection>" << endl;
				configFile << "\t\t\t\t\t<refraction>" << this->Scene->Object[i]->GetMaterialProperty()->GetRefraction() << "</refraction>" << endl;
				configFile << "\t\t\t\t\t<density>" << this->Scene->Object[i]->GetMaterialProperty()->GetDensity() << "</density>" << endl;
            configFile << "\t\t\t\t\t<power>" << this->Scene->Object[i]->GetMaterialProperty()->GetPower() << "</power>" << endl;
            configFile << "\t\t\t\t</materialproperty>" << endl;
            configFile << "\t\t\t</object>" << endl;

            break;
         default :
            configFile << "\t\t\t<object id=\"" << i+1 << "\" type=\"undefined\">" << endl;
            configFile << "\t\t\t</object>" << endl;
      }
   }
   configFile << "\t\t</objects>" << endl;

   configFile << "\t\t<lights>" << endl;
   for (i = 0; i < this->Scene->Light.size(); i++)
   {
      configFile << "\t\t\t<light id=\"" << i+1 << "\">" << endl;
      configFile << "\t\t\t\t<origin>" << this->Scene->Light[i]->GetOrigin() << "</origin>" << endl;
      configFile << "\t\t\t\t<intensity>" << this->Scene->Light[i]->GetIntensity() << "</intensity>" << endl;
      configFile << "\t\t\t</light>" << endl;
   }
   configFile << "\t\t</lights>" << endl;

   configFile << "\t</image>" << endl;
   configFile << "</inputset>" << endl;

   configFile.close();

	return true;
}
//------------------------------------------------------------------------------
bool TViewport::ConfigureFromFile(char *filename)
{
   XMLElement *ele, *nele, *ob;
   char name[50], value[50];
   int i, j, chi, chj;
   float v3f[3], w3f[3];

   XML* inXml = new XML(filename);
   ele = inXml->GetRootElement()->GetChildren()[0];
   chi = ele->GetChildrenNum();

   for(i = 0; i < chi; i++)
   {
      nele = ele->GetChildren()[i];
      nele->GetElementName(name, 0);

      if(!strcmp(name, "width"))
      {
         nele->GetContents()[0]->GetValue(value, 0);
         this->SetWidth(atoi(value));
      }
      else if(!strcmp(name, "height"))
      {
         nele->GetContents()[0]->GetValue(value, 0);
         this->SetHeight(atoi(value));
      }
      else if(!strcmp(name, "objects"))
      {
         chj = nele->GetChildrenNum();
         for (j = 0; j < chj; j++)
         {
            // Get object type
            nele->GetChildren()[j]->GetVariables()[1]->GetValue(name, 0);
            if(!strcmp(name, "sphere"))
            {
               // Get Shape
               ob = nele->GetChildren()[j]->GetChildren()[0];
               ob->GetChildren()[0]->GetContents()[0]->GetValue(value, 0);
               StrToVector3f(value, v3f);
               ob->GetChildren()[1]->GetContents()[0]->GetValue(value, 0);

               TObject *lObject = new TObject;
               TMaterialProperty *lMP = new TMaterialProperty;
               TShape *lSphere = new TSphere(TPoint(v3f[0], v3f[1], v3f[2]), atof(value));
               // Got Shape

               // Get Material Property
               ob = nele->GetChildren()[j]->GetChildren()[1];
               // Ambient
               ob->GetChildren()[0]->GetContents()[0]->GetValue(value, 0);
               StrToVector3f(value, v3f);
               lMP->SetAmbient(TIntensity(v3f[0], v3f[1], v3f[2]));

               // Diffusion
               ob->GetChildren()[1]->GetContents()[0]->GetValue(value, 0);
               StrToVector3f(value, v3f);
               lMP->SetDiffusion(TIntensity(v3f[0], v3f[1], v3f[2]));

               // Specular
               ob->GetChildren()[2]->GetContents()[0]->GetValue(value, 0);
               StrToVector3f(value, v3f);
               lMP->SetSpecular(TIntensity(v3f[0], v3f[1], v3f[2]));

               // Shininess
               ob->GetChildren()[3]->GetContents()[0]->GetValue(value, 0);
               StrToVector3f(value, v3f);
               lMP->SetShininess(TIntensity(v3f[0], v3f[1], v3f[2]));

               // Emission
               ob->GetChildren()[4]->GetContents()[0]->GetValue(value, 0);
               StrToVector3f(value, v3f);
               lMP->SetEmission(TIntensity(v3f[0], v3f[1], v3f[2]));

               // Reflection
               ob->GetChildren()[5]->GetContents()[0]->GetValue(value, 0);
               lMP->SetReflection(atof(value));

               // Refraction
               ob->GetChildren()[6]->GetContents()[0]->GetValue(value, 0);
               lMP->SetRefraction(atof(value));

               // Density
               ob->GetChildren()[7]->GetContents()[0]->GetValue(value, 0);
               lMP->SetDensity(atof(value));

               // Power
               ob->GetChildren()[8]->GetContents()[0]->GetValue(value, 0);
               lMP->SetPower(atoi(value));
               // Got Material Property

               // Add Object to Scene
               lObject->SetShape(lSphere);
               lObject->SetMaterialProperty(lMP);
               this->Scene->AddObject(lObject);
               // Added
            }
         }
      }
      else if(!strcmp(name, "lights"))
      {
         chj = nele->GetChildrenNum();
         for (j = 0; j < chj; j++)
         {
            ob = nele->GetChildren()[j]->GetChildren()[0];
            ob->GetContents()[0]->GetValue(value, 0);
            StrToVector3f(value, v3f);

            ob = nele->GetChildren()[j]->GetChildren()[1];
            ob->GetContents()[0]->GetValue(value, 0);
            StrToVector3f(value, w3f);

            this->Scene->AddLight(TPoint(v3f[0], v3f[1], v3f[2]), TIntensity(w3f[0], w3f[1], w3f[2]));
         }
      }
   }

   delete inXml;

   return true;
}
//------------------------------------------------------------------------------
bool TViewport::SaveLoadToFile(char *filename, int size, bool breakdown)
{
   ofstream loadFile(filename);
   if (!loadFile)
      return false;

#if defined(SRAY_MPI) && defined(SRAY_LB1)

	if(!breakdown)
	{
		int *segarray = new int [size+1];
		int *offarray = new int [size];

		int rem = this->Height%size, i;

		segarray[0] = -1;
		for(i = 1; i <= size; i++)
		{
			if(i < rem)
				segarray[i] = this->Height/size + 1 + segarray[i-1];
			else
				segarray[i] = this->Height/size + segarray[i-1];
			offarray[i-1] = 0;
		}

		int count = 0, y, x;
		for(i = size; count < this->Height; i = (i-1)+(i==1)*size)
		{
			y = segarray[i] - offarray[i-1]; 
			if(y <= segarray[i-1])
				continue;
			count++;
			offarray[i-1]++;
			for(x = 0; x < this->Width; x++)
			{
				loadFile << this->LoadMatrix[y*this->Width + x] << " ";
			}
			loadFile << endl;
		}

		delete [] offarray;
		delete [] segarray;
	}
	else
	{
	   for(int y = this->Height - 1; y >= 0; y--)
	   {
		  for(int x = 0; x < this->Width; x++)
		  {
			 loadFile << this->LoadMatrix[y*Width + x] << " ";
		  }
		  loadFile << endl;
	   }
	}
#else

   for(int y = this->Height - 1; y >= 0; y--)
   {
      for(int x = 0; x < this->Width; x++)
	  {
         loadFile << this->LoadMatrix[y*Width + x] << " ";
      }
      loadFile << endl;
   }
#endif

	loadFile.close();

	return true;
}
//------------------------------------------------------------------------------
bool TViewport::AllocateMemory(int starty, int endy, int yincr, int rank, int size)
{
#ifdef SRAY_MPI
#	ifdef SRAY_LB1
	this->locHeight = 0;
	int rem = this->Height%size;

	if(rank < rem)
		this->locHeight = this->Height/size + 1;
	else
		this->locHeight = this->Height/size;
#	else
	this->locHeight = endy - starty + 1;
#	endif
#endif

int datasize = this->Width * this->Height; 
#ifdef SRAY_MPI
	datasize = this->Width * this->locHeight;	
	if(!MetadataCollection(datasize, rank, size))
		return false;
	
#endif

#ifdef SRAY_OPENMP
#	pragma omp master
#endif
	{
		this->ImageMatrix = new COLOR [datasize * 3];
#ifdef LOAD_MEASUREMENT
		this->LoadMatrix = new int [datasize];
#endif
	}

#ifdef SRAY_OPENMP
#	pragma omp barrier   
#endif                  

   return true;
}
//------------------------------------------------------------------------------
bool TViewport::MetadataCollection(int &datasize, int rank, int size)
{
#ifndef SRAY_MPI
	return false;
#else
	
	if((storeRcvDatasize = (int *) malloc(sizeof(int)*(size))) == NULL)
        return false;

	int i, bitval, tag = 100;
	int tempdatasize;

	MPI_Status	status;

	bitval = 1;

	while (bitval < size)
	{
		if (bitval & rank)
		{
			MPI_Send(&datasize, 1, MPI_INTEGER, (rank^bitval), tag, MPI_COMM_WORLD);
			break;
		}
		else
		{
			if ((rank^bitval) < size)
			{
				MPI_Recv(&tempdatasize, 1, MPI_INTEGER, (rank^bitval), tag,
	               MPI_COMM_WORLD, &status);

				this->storeRcvDatasize[rank^bitval] = tempdatasize;

				datasize += tempdatasize;
			}
		}
		bitval = bitval<<1;
	}

//	storeMaxDatasize = datasize;
	return true;
#endif
}
//------------------------------------------------------------------------------
bool TViewport::DataCollection(int rank, int size)
{
#ifndef SRAY_MPI
	return false;
#else

	int i, bitval, tag = 102;
	int datasize = this->Width * this->locHeight * 3;

	MPI_Status	status;

	bitval = 1;

	while (bitval < size)
	{
		if (bitval & rank)
		{
			MPI_Send(this->ImageMatrix, datasize, MPI_CHAR, (rank^bitval), tag, MPI_COMM_WORLD);
			break;
		}
		else
		{
			if ((rank^bitval) < size)
			{
				MPI_Recv(&(this->ImageMatrix[datasize]), this->storeRcvDatasize[rank^bitval]*3,
					MPI_CHAR, (rank^bitval), tag, MPI_COMM_WORLD, &status);

				datasize += this->storeRcvDatasize[rank^bitval]*3;
			}
		}
		bitval = bitval<<1;
	}

	return true;
#endif
}
//------------------------------------------------------------------------------
bool TViewport::LoadDataCollection(int rank, int size)
{
#ifndef SRAY_MPI
	return false;
#else

	int i, bitval, tag = 101;
	int datasize = this->Width * this->locHeight;

	MPI_Status	status;

	bitval = 1;

	while (bitval < size)
	{
		if (bitval & rank)
		{
			MPI_Send(this->LoadMatrix, datasize, MPI_INT, (rank^bitval), tag, MPI_COMM_WORLD);
//			cout << "Rank# " << rank << ": Send to " << (rank^bitval) << ", Datasize " << datasize << endl;
			break;
		}
		else
		{
			if ((rank^bitval) < size)
			{
				MPI_Recv(&(this->LoadMatrix[datasize]), this->storeRcvDatasize[rank^bitval],
					MPI_INT, (rank^bitval), tag, MPI_COMM_WORLD, &status);

//				cout << "Rank# " << rank << ": Recv from " << (rank^bitval) << ", Datasize " << this->storeRcvDatasize[rank^bitval] << endl;
				datasize += this->storeRcvDatasize[rank^bitval];
//				cout << "Rank# " << rank << ": Sum up Datasize " << datasize << endl;
			}
		}
		bitval = bitval<<1;
	}

	return true;
#endif
}
//------------------------------------------------------------------------------
